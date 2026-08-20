const fs = require('fs');
const path = require('path');
const deviceBridge = require('./device_bridge');

class FileTransferEngine {
    constructor() {
        this.chunkSize = 64 * 1024; // 64 KB chunks
        this.defaultWorkspace = '/data/local/tmp/mem_workspace/';
    }

    /**
     * Generate unique local file path with timestamp/counter to never overwrite existing files
     */
    getUniqueLocalPath(targetPath) {
        if (!fs.existsSync(targetPath)) return targetPath;
        const dir = path.dirname(targetPath);
        const ext = path.extname(targetPath);
        const baseName = path.basename(targetPath, ext);
        
        let counter = 1;
        let candidate = path.join(dir, `${baseName}_(${counter})${ext}`);
        while (fs.existsSync(candidate)) {
            counter++;
            candidate = path.join(dir, `${baseName}_(${counter})${ext}`);
        }
        return candidate;
    }

    /**
     * Generate unique remote file path on Android to avoid collisions
     */
    async getUniqueRemotePath(remotePath) {
        const stat = await this.statFile(remotePath);
        if (!stat.exists) return remotePath;

        const slash = remotePath.lastIndexOf('/');
        const dir = slash !== -1 ? remotePath.substring(0, slash + 1) : '/data/local/tmp/';
        const fullName = slash !== -1 ? remotePath.substring(slash + 1) : remotePath;
        const dot = fullName.lastIndexOf('.');
        const baseName = dot !== -1 ? fullName.substring(0, dot) : fullName;
        const ext = dot !== -1 ? fullName.substring(dot) : '';

        let counter = 1;
        let candidate = `${dir}${baseName}_(${counter})${ext}`;
        let candStat = await this.statFile(candidate);
        while (candStat.exists) {
            counter++;
            candidate = `${dir}${baseName}_(${counter})${ext}`;
            candStat = await this.statFile(candidate);
        }
        return candidate;
    }

    /**
     * List remote directory
     */
    async listDirectory(dirPath = '/data/local/tmp/') {
        return deviceBridge.sendCommand(`fs_list ${dirPath}`);
    }

    /**
     * Get stat of remote file
     */
    async statFile(filePath) {
        return deviceBridge.sendCommand(`fs_stat ${filePath}`);
    }

    /**
     * Download a remote file to local filesystem chunk by chunk (safe non-destructive)
     */
    async downloadFile(remotePath, localPath, overwrite = false, progressCallback = null) {
        const statRes = await this.statFile(remotePath);
        if (statRes.status !== 'ok') {
            throw new Error(`Remote file does not exist: ${remotePath}`);
        }

        let finalLocalPath = localPath;
        if (!overwrite) {
            finalLocalPath = this.getUniqueLocalPath(localPath);
        }

        const totalSize = statRes.size || 0;
        let offset = 0;
        let isEof = false;

        // Ensure parent directory exists
        const parentDir = path.dirname(finalLocalPath);
        if (!fs.existsSync(parentDir)) {
            fs.mkdirSync(parentDir, { recursive: true });
        }

        const writeStream = fs.createWriteStream(finalLocalPath);

        const startTime = Date.now();

        while (!isEof) {
            const res = await deviceBridge.sendCommand(`fs_read ${remotePath} ${offset} ${this.chunkSize}`);
            if (res.status !== 'ok') {
                writeStream.close();
                throw new Error(`Failed to read chunk at offset ${offset}: ${res.message}`);
            }

            const buf = Buffer.from(res.data_b64, 'base64');
            writeStream.write(buf);

            offset += res.bytes_read;
            isEof = res.is_eof || res.bytes_read === 0;

            if (progressCallback && totalSize > 0) {
                const percent = Math.min(100, Math.round((offset / totalSize) * 100));
                const elapsed = (Date.now() - startTime) / 1000;
                const speedMBs = elapsed > 0 ? (offset / 1024 / 1024 / elapsed).toFixed(2) : 0;
                progressCallback({ offset, totalSize, percent, speedMBs, file: path.basename(finalLocalPath) });
            }
        }

        return new Promise((resolve, reject) => {
            writeStream.end(() => {
                resolve({
                    status: 'ok',
                    localPath: finalLocalPath,
                    remotePath,
                    totalBytes: offset,
                    message: `Downloaded ${offset} bytes successfully without modifying existing files`
                });
            });
            writeStream.on('error', reject);
        });
    }

    /**
     * Upload a local file to remote Android device chunk by chunk (safe non-destructive)
     */
    async uploadFile(localPath, remotePath, overwrite = false, progressCallback = null) {
        if (!fs.existsSync(localPath)) {
            throw new Error(`Local file not found: ${localPath}`);
        }

        let finalRemotePath = remotePath;
        if (!overwrite) {
            finalRemotePath = await this.getUniqueRemotePath(remotePath);
        }

        const stats = fs.statSync(localPath);
        const totalSize = stats.size;
        let offset = 0;

        const fd = fs.openSync(localPath, 'r');
        const buf = Buffer.alloc(this.chunkSize);

        const startTime = Date.now();
        let isFirst = true;

        while (offset < totalSize) {
            const bytesRead = fs.readSync(fd, buf, 0, this.chunkSize, offset);
            if (bytesRead === 0) break;

            const b64 = buf.subarray(0, bytesRead).toString('base64');
            const res = await deviceBridge.sendCommand(`fs_write ${finalRemotePath} ${offset} ${isFirst ? '1' : '0'} ${b64}`);

            if (res.status !== 'ok') {
                fs.closeSync(fd);
                throw new Error(`Failed to write chunk at offset ${offset}: ${res.message}`);
            }

            offset += bytesRead;
            isFirst = false;

            if (progressCallback) {
                const percent = Math.min(100, Math.round((offset / totalSize) * 100));
                const elapsed = (Date.now() - startTime) / 1000;
                const speedMBs = elapsed > 0 ? (offset / 1024 / 1024 / elapsed).toFixed(2) : 0;
                progressCallback({ offset, totalSize, percent, speedMBs, file: path.basename(finalRemotePath) });
            }
        }

        fs.closeSync(fd);

        return {
            status: 'ok',
            localPath,
            remotePath: finalRemotePath,
            totalBytes: offset,
            message: `Uploaded ${offset} bytes to ${finalRemotePath} safely`
        };
    }

    /**
     * Download remote directory safely into dedicated timestamped folder/zip
     */
    async downloadDirectory(remoteDir, localZipPath, progressCallback = null) {
        const cleanDirName = path.basename(remoteDir.replace(/\/$/, '')) || 'folder';
        const tempZipName = `temp_${cleanDirName}_${Date.now()}.zip`;
        const remoteZipPath = `/data/local/tmp/${tempZipName}`;

        // 1. Compress on device safely
        const compRes = await deviceBridge.sendCommand(`compress ${remoteDir} ${remoteZipPath} zip`);
        if (compRes.status !== 'ok') {
            throw new Error(`Failed to compress remote directory: ${compRes.message}`);
        }

        // 2. Download zip file non-destructively
        const downRes = await this.downloadFile(remoteZipPath, localZipPath, false, progressCallback);

        // 3. Cleanup temp zip on device
        await deviceBridge.sendCommand(`fs_delete ${remoteZipPath} 0`);

        return downRes;
    }

    /**
     * Delete file or directory on device
     */
    async deleteItem(remotePath, recursive = false) {
        return deviceBridge.sendCommand(`fs_delete ${remotePath} ${recursive ? '1' : '0'}`);
    }

    /**
     * Rename file or directory on device
     */
    async renameItem(oldPath, newPath) {
        return deviceBridge.sendCommand(`fs_rename ${oldPath} ${newPath}`);
    }

    /**
     * Create directory on device
     */
    async makeDir(remoteDir) {
        return deviceBridge.sendCommand(`fs_mkdir ${remoteDir}`);
    }
}

module.exports = new FileTransferEngine();
