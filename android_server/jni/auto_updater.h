#ifndef AUTO_UPDATER_H
#define AUTO_UPDATER_H

#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>

class AutoUpdater {
public:
    /**
     * Test and apply update
     * @param update_source_path Path to the uploaded new binary (e.g. /data/local/tmp/updates/mem_server_new.sh)
     * @param target_live_path Path to the live server binary (e.g. /data/local/tmp/mem_server.sh)
     * @param out_err Error message output if validation or test execution fails
     * @return true if new binary is validated and safely launched; false with error if test fails
     */
    static bool apply_update(const std::string& update_source_path,
                             const std::string& target_live_path,
                             int current_port,
                             std::string& out_err);

    /**
     * Test execution of candidate binary with --help or --test in background
     * Returns true if candidate starts and exits with code 0 without crashing
     */
    static bool test_binary(const std::string& binary_path, std::string& out_err);
};

#endif // AUTO_UPDATER_H
