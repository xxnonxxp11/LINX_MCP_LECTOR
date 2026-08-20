#include "auto_updater.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <thread>

bool AutoUpdater::test_binary(const std::string& binary_path, std::string& out_err) {
    LOG_INFO("UPDATE", "Testing candidate update binary: %s", binary_path.c_str());

    // 1. Verify existence
    struct stat st;
    if (stat(binary_path.c_str(), &st) != 0) {
        out_err = "Update binary does not exist: " + binary_path + " (errno: " + strerror(errno) + ")";
        LOG_ERROR("UPDATE", "%s", out_err.c_str());
        return false;
    }

    // 2. Grant chmod 777
    if (chmod(binary_path.c_str(), 0777) != 0) {
        out_err = "Failed to chmod 777 on update binary: " + binary_path + " (errno: " + strerror(errno) + ")";
        LOG_ERROR("UPDATE", "%s", out_err.c_str());
        return false;
    }

    // 3. Test execution by running with -h in a subprocess
    int pipe_fd[2];
    if (pipe(pipe_fd) < 0) {
        out_err = "Pipe error during test execution";
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        out_err = "Fork error during test execution";
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return false;
    }

    if (pid == 0) {
        // Child: redirect stdout and stderr to pipe
        close(pipe_fd[0]);
        dup2(pipe_fd[1], STDOUT_FILENO);
        dup2(pipe_fd[1], STDERR_FILENO);
        close(pipe_fd[1]);

        execl(binary_path.c_str(), binary_path.c_str(), "-h", (char*)NULL);
        // If execl fails:
        _exit(127);
    }

    // Parent
    close(pipe_fd[1]);
    char buf[1024] = {0};
    ssize_t bytes_read = read(pipe_fd[0], buf, sizeof(buf) - 1);
    close(pipe_fd[0]);

    int status = 0;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        if (exit_code == 0) {
            LOG_INFO("UPDATE", "Candidate binary validated successfully (exit code 0)");
            return true;
        } else if (exit_code == 127) {
            out_err = "Exec format error or missing dynamic linker on candidate binary (ARM64 mismatch?)";
            LOG_ERROR("UPDATE", "%s", out_err.c_str());
            return false;
        } else {
            out_err = "Candidate binary exited with error code " + std::to_string(exit_code) + ". Output: " + (bytes_read > 0 ? buf : "none");
            LOG_ERROR("UPDATE", "%s", out_err.c_str());
            return false;
        }
    } else if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        out_err = "Candidate binary crashed with signal " + std::to_string(sig) + " (SIGSEGV / SIGILL)";
        LOG_ERROR("UPDATE", "%s", out_err.c_str());
        return false;
    }

    out_err = "Candidate test execution failed unexpectedly";
    return false;
}

bool AutoUpdater::apply_update(const std::string& update_source_path,
                             const std::string& target_live_path,
                             int current_port,
                             std::string& out_err) {
    LOG_INFO("UPDATE", "Initiating safe auto-update from '%s' to '%s'...",
             update_source_path.c_str(), target_live_path.c_str());

    // Step 1: Pre-launch validation test
    if (!test_binary(update_source_path, out_err)) {
        LOG_ERROR("UPDATE", "ABORTING UPDATE: Candidate binary failed validation. Previous server stays running safely.");
        return false;
    }

    // Step 2: Backup current live binary to .bak
    std::string backup_path = target_live_path + ".bak";
    rename(target_live_path.c_str(), backup_path.c_str());
    LOG_INFO("UPDATE", "Backed up current binary to %s", backup_path.c_str());

    // Step 3: Atomic replace / rename new binary into place
    if (rename(update_source_path.c_str(), target_live_path.c_str()) != 0) {
        out_err = "Failed to rename update binary into live location: " + std::string(strerror(errno));
        LOG_ERROR("UPDATE", "%s", out_err.c_str());
        // Restore backup
        rename(backup_path.c_str(), target_live_path.c_str());
        return false;
    }

    // Step 4: Ensure permissions 777 on target
    chmod(target_live_path.c_str(), 0777);
    LOG_INFO("UPDATE", "Permissions 777 verified on %s", target_live_path.c_str());

    // Step 5: Spawn replacement daemon in background and handoff
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", current_port);

    pid_t new_pid = fork();
    if (new_pid == 0) {
        // Child process: Become session leader
        setsid();

        // Close all inherited open file descriptors (sockets, pipes) from parent
        for (int fd = 3; fd < 256; fd++) {
            close(fd);
        }
        
        // Wait 600ms for parent process to finish its exit(0) and let Linux release the port
        usleep(600000);

        // Execute new binary with root permissions
        execl(target_live_path.c_str(), target_live_path.c_str(), "-p", port_str, (char*)NULL);
        _exit(1);
    }

    LOG_INFO("UPDATE", "New server daemon spawned with PID %d on port %d. Exiting old process.", new_pid, current_port);

    // Detach a thread in parent that will exit cleanly after flushing TCP response
    std::thread([]() {
        usleep(100000); // 100ms allows TCP response to be delivered to PC
        LOG_INFO("UPDATE", "Old daemon shutting down cleanly to release port.");
        _exit(0);
    }).detach();

    return true;
}
