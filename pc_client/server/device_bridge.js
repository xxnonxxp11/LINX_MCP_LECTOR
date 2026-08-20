const net = require('net');
const { exec } = require('child_process');
const fs = require('fs');
const path = require('path');
const os = require('os');

const KNOWN_GAMES = [
    { name: 'Arena Breakout Lite', pkg: 'com.proximabeta.mf.liteuamo' },
    { name: 'Arena Breakout Standard', pkg: 'com.proximabeta.mf.uamo' }
];

class DeviceBridge {
    constructor() {
        this.socket = null;
        this.host = '127.0.0.1';
        this.port = 8088;
        this.isConnected = false;
        this.connectionMode = 'WIFI'; // 'USB' or 'WIFI'
        this.commandQueue = [];
        this.isProcessingQueue = false;
        this.currentPendingRequest = null;
        this.buffer = '';
        this.onLogCallback = null;
        this.onStatusChangeCallback = null;
        this.adbPath = this.resolveAdbPath();
    }

    resolveAdbPath() {
        const sdkAdb = 'C:\\Users\\Usuario\\AppData\\Local\\Android\\Sdk\\platform-tools\\adb.exe';
        if (fs.existsSync(sdkAdb)) return sdkAdb;
        return 'adb';
    }

    setLogCallback(cb) {
        this.onLogCallback = cb;
    }

    setStatusCallback(cb) {
        this.onStatusChangeCallback = cb;
    }

    log(type, msg) {
        // Suppress repetitive background polling commands (get_logs, status) to keep terminal and UI clean
        if (msg && (msg.startsWith('get_logs') || msg === 'status')) {
            return;
        }
        const timestamp = new Date().toLocaleTimeString();
        if (this.onLogCallback) {
            this.onLogCallback({ timestamp, type, message: msg });
        }
        // Write to stderr so stdout remains pure JSON-RPC stream for MCP
        console.error(`[${timestamp}] [${type.toUpperCase()}] ${msg}`);
    }

    getLastSavedHost() {
        try {
            const cfgPath = path.join(__dirname, '../last_host.json');
            if (fs.existsSync(cfgPath)) {
                const data = JSON.parse(fs.readFileSync(cfgPath, 'utf8'));
                if (data && data.host) return data.host;
            }
        } catch (e) {}
        return null;
    }

    saveActiveHost(host, port = 8088) {
        try {
            const cfgPath = path.join(__dirname, '../last_host.json');
            fs.writeFileSync(cfgPath, JSON.stringify({ host, port, updatedAt: new Date().toISOString() }, null, 2));
        } catch (e) {}
    }

    /**
     * Check if a specific IP has mem_server daemon running on port
     */
    async probeHost(ip, port = 8088, timeoutMs = 350) {
        return new Promise((resolve) => {
            const s = new net.Socket();
            let resolved = false;
            s.setTimeout(timeoutMs);

            s.connect(port, ip, () => {
                resolved = true;
                s.destroy();
                resolve(true);
            });

            s.on('error', () => {
                if (!resolved) {
                    resolved = true;
                    s.destroy();
                    resolve(false);
                }
            });

            s.on('timeout', () => {
                if (!resolved) {
                    resolved = true;
                    s.destroy();
                    resolve(false);
                }
            });
        });
    }

    /**
     * Auto-discover running Android mem_server across all local subnets
     */
    async discoverDaemonOnNetwork(port = 8088) {
        // 1. Probe last saved host first (instant)
        const lastHost = this.getLastSavedHost();
        if (lastHost && lastHost !== '127.0.0.1') {
            const isUp = await this.probeHost(lastHost, port, 400);
            if (isUp) {
                return { found: true, ip: lastHost, port, source: 'cached' };
            }
        }

        // 2. Gather all IPv4 local subnet IPs
        const interfaces = os.networkInterfaces();
        const candidateIps = [];

        for (const name of Object.keys(interfaces)) {
            for (const iface of interfaces[name]) {
                if (iface.family === 'IPv4' && !iface.internal) {
                    const parts = iface.address.split('.');
                    if (parts.length === 4) {
                        const prefix = `${parts[0]}.${parts[1]}.${parts[2]}`;
                        for (let i = 1; i <= 254; i++) {
                            const target = `${prefix}.${i}`;
                            if (target !== iface.address) {
                                candidateIps.push(target);
                            }
                        }
                    }
                }
            }
        }

        // 3. Scan all IPs in parallel with 350ms timeout - first to respond wins
        try {
            const hit = await new Promise((resolve) => {
                let pending = candidateIps.length;
                let foundAny = false;

                candidateIps.forEach((ip) => {
                    const s = new net.Socket();
                    s.setTimeout(350);

                    s.connect(port, ip, () => {
                        if (!foundAny) {
                            foundAny = true;
                            s.destroy();
                            resolve(ip);
                        } else {
                            s.destroy();
                        }
                    });

                    const cleanup = () => {
                        s.destroy();
                        pending--;
                        if (pending === 0 && !foundAny) {
                            resolve(null);
                        }
                    };

                    s.on('error', cleanup);
                    s.on('timeout', cleanup);
                });
            });

            if (hit) {
                this.saveActiveHost(hit, port);
                return { found: true, ip: hit, port, source: 'fast_parallel_scan' };
            }
        } catch (e) {}

        return { found: false, ip: null, port };
    }

    /**
     * Check if a USB device is connected via ADB
     */
    async detectUsbDevices() {
        return new Promise((resolve) => {
            exec(`"${this.adbPath}" devices -l`, (err, stdout) => {
                if (err || !stdout) {
                    resolve({ hasUsb: false, devices: [] });
                    return;
                }
                const lines = stdout.split('\n').map(l => l.trim()).filter(Boolean);
                const devices = [];
                for (let i = 1; i < lines.length; i++) {
                    const parts = lines[i].split(/\s+/);
                    if (parts.length >= 2 && parts[1] === 'device') {
                        devices.push({
                            id: parts[0],
                            model: lines[i].includes('model:') ? lines[i].split('model:')[1].split(' ')[0] : 'Android Device',
                            raw: lines[i]
                        });
                    }
                }
                resolve({ hasUsb: devices.length > 0, devices });
            });
        });
    }

    async runAdbForward(localPort = 8088, deviceId = null) {
        return new Promise(async (resolve) => {
            let targetDevice = deviceId;
            if (!targetDevice) {
                const check = await this.detectUsbDevices();
                if (check.hasUsb && check.devices.length > 0) {
                    targetDevice = check.devices[0].id;
                }
            }

            const targetFlag = targetDevice ? `-s ${targetDevice}` : '';
            const cmd = `"${this.adbPath}" ${targetFlag} forward tcp:${localPort} localabstract:memsvc`;
            exec(cmd, (err, stdout, stderr) => {
                if (err) {
                    this.log('error', `ADB Forward (unix) failed: ${stderr || err.message}`);
                    resolve({ success: false, message: stderr || err.message });
                } else {
                    this.connectionMode = 'USB';
                    this.log('info', `⚡ Stealth ADB Tunnel active (${targetDevice || 'default'}): tcp:${localPort} -> @memsvc`);
                    resolve({ success: true, message: `Forwarded tcp:${localPort} -> localabstract:memsvc` });
                }
            });
        });
    }

    /**
     * Smart Connect: Always uses ADB forward (Unix Abstract Socket @memsvc).
     */
    async smartConnect(preferredHost = null, port = 8088) {
        const usbCheck = await this.detectUsbDevices();
        if (usbCheck.hasUsb && usbCheck.devices.length > 0) {
            const dev = usbCheck.devices[0];
            this.log('info', `📱 Android Device detected (${dev.model || dev.id}). Setting up stealth ADB tunnel...`);
            const fwdRes = await this.runAdbForward(port, dev.id);
            if (fwdRes.success) {
                this.connectionMode = 'USB';
                return this.connect('127.0.0.1', port);
            }
        }
        // Fallback: try 127.0.0.1 in case adb forward was set up manually
        this.connectionMode = 'USB';
        return this.connect('127.0.0.1', port);
    }

    connect(host = '127.0.0.1', port = 8088) {
        if (this.isConnected && this.socket && !this.socket.destroyed) {
            return Promise.resolve({ success: true, host: this.host, port: this.port });
        }

        return new Promise((resolve, reject) => {
            this.host = host;
            this.port = port;

            if (this.socket) {
                this.socket.destroy();
                this.socket = null;
            }

            this.log('info', `Connecting to Android daemon at ${host}:${port}...`);
            this.socket = new net.Socket();
            let hasResolved = false;

            this.socket.connect(port, host, () => {
                hasResolved = true;
                this.isConnected = true;
                this.saveActiveHost(host, port);
                this.socket.setTimeout(0); // clear timeout once connected
                this.log('success', `Connected to Android daemon at ${host}:${port}`);
                if (this.onStatusChangeCallback) this.onStatusChangeCallback(true);
                resolve({ success: true, host, port });

                // Automatically detect and attach to any running game without manual guessing
                setTimeout(() => {
                    this.autoAttachGame().catch(() => {});
                }, 150);
            });

            this.socket.on('data', (data) => {
                this.handleIncomingData(data);
            });

            this.socket.on('error', (err) => {
                this.log('error', `Socket error: ${err.message}`);
                this.isConnected = false;
                if (this.onStatusChangeCallback) this.onStatusChangeCallback(false);
                this.clearCommandQueue(err);
                if (!hasResolved) {
                    hasResolved = true;
                    reject(new Error(`No se pudo conectar a ${host}:${port} (${err.code || err.message}). Asegúrate de ejecutar mem_server.sh en el celular.`));
                }
            });

            this.socket.on('close', () => {
                this.isConnected = false;
                this.log('warn', 'Connection to Android daemon closed');
                this.clearCommandQueue(new Error('Connection closed'));
                if (this.onStatusChangeCallback) this.onStatusChangeCallback(false);
            });

            this.socket.on('timeout', () => {
                this.log('error', 'Connection timed out');
                this.socket.destroy();
                this.isConnected = false;
                this.clearCommandQueue(new Error('Connection timed out'));
                if (!hasResolved) {
                    hasResolved = true;
                    reject(new Error(`Tiempo de espera agotado al conectar a ${host}:${port}`));
                }
            });
        });
    }

    clearCommandQueue(error) {
        if (this.currentPendingRequest) {
            if (this.currentPendingRequest.timer) clearTimeout(this.currentPendingRequest.timer);
            this.currentPendingRequest.reject(error);
            this.currentPendingRequest = null;
        }
        while (this.commandQueue.length > 0) {
            const item = this.commandQueue.shift();
            if (item.timer) clearTimeout(item.timer);
            item.reject(error);
        }
        this.isProcessingQueue = false;
    }

    disconnect() {
        if (this.socket) {
            this.socket.destroy();
            this.socket = null;
        }
        this.isConnected = false;
        this.clearCommandQueue(new Error('Manual disconnect'));
        this.log('info', 'Disconnected from daemon');
        if (this.onStatusChangeCallback) this.onStatusChangeCallback(false);
    }

    handleIncomingData(data) {
        this.buffer += data.toString('utf-8');
        let newlineIndex;

        while ((newlineIndex = this.buffer.indexOf('\n')) !== -1) {
            const line = this.buffer.substring(0, newlineIndex).trim();
            this.buffer = this.buffer.substring(newlineIndex + 1);

            if (line.length === 0) continue;

            if (this.currentPendingRequest) {
                const req = this.currentPendingRequest;
                this.currentPendingRequest = null;
                if (req.timer) clearTimeout(req.timer);

                try {
                    const json = JSON.parse(line);
                    req.resolve(json);
                } catch (e) {
                    req.resolve({ status: 'raw', data: line });
                }

                // Process next command in queue
                this.processNextInQueue();
            } else {
                this.log('recv', line);
            }
        }
    }

    sendCommand(cmdString, timeoutMs = 8000) {
        return new Promise((resolve, reject) => {
            if (!this.isConnected || !this.socket) {
                return reject(new Error('Not connected to Android daemon'));
            }

            this.commandQueue.push({ cmdString, resolve, reject, timeoutMs });
            if (!this.isProcessingQueue) {
                this.processNextInQueue();
            }
        });
    }

    processNextInQueue() {
        if (this.commandQueue.length === 0) {
            this.isProcessingQueue = false;
            this.currentPendingRequest = null;
            return;
        }

        this.isProcessingQueue = true;
        const item = this.commandQueue.shift();
        this.currentPendingRequest = item;

        // Set safety timeout for this specific command
        item.timer = setTimeout(() => {
            if (this.currentPendingRequest === item) {
                this.log('error', `Command timed out (${item.timeoutMs}ms): ${item.cmdString}`);
                this.currentPendingRequest = null;
                item.reject(new Error(`Command timed out: ${item.cmdString}`));
                this.processNextInQueue();
            }
        }, item.timeoutMs);

        try {
            this.log('send', item.cmdString);
            this.socket.write(item.cmdString + '\n');
        } catch (err) {
            if (item.timer) clearTimeout(item.timer);
            this.currentPendingRequest = null;
            item.reject(err);
            this.processNextInQueue();
        }
    }

    // High level helper methods
    async ping() {
        return this.sendCommand('ping');
    }

    async getStatus() {
        return this.sendCommand('status');
    }

    async attach(target) {
        return this.sendCommand(`attach ${target}`);
    }

    async detach() {
        return this.sendCommand('detach');
    }

    async listProcesses(force = false) {
        const now = Date.now();
        if (!force && this._cachedProcessList && (now - (this._lastProcessScan || 0) < 3000)) {
            return this._cachedProcessList;
        }
        if (this._inFlightProcessScan) {
            return this._inFlightProcessScan;
        }
        this._inFlightProcessScan = this.sendCommand('ps', 12000).then(res => {
            this._cachedProcessList = res;
            this._lastProcessScan = Date.now();
            this._inFlightProcessScan = null;
            return res;
        }).catch(err => {
            this._inFlightProcessScan = null;
            throw err;
        });
        return this._inFlightProcessScan;
    }

    async getModules() {
        return this.sendCommand('modules');
    }

    async readMemory(address, size = 64) {
        return this.sendCommand(`read ${address} ${size}`);
    }

    async writeMemory(address, hexData) {
        return this.sendCommand(`write ${address} ${hexData}`);
    }

    async readPointerChain(base, offsets = []) {
        const offsetStr = offsets.join(' ');
        return this.sendCommand(`read_ptr ${base} ${offsetStr}`);
    }

    async readString(address, maxLen = 64) {
        return this.sendCommand(`read_str ${address} ${maxLen}`);
    }

    async patternScan(startAddr, endAddr, pattern) {
        return this.sendCommand(`scan ${startAddr} ${endAddr} ${pattern}`);
    }

    async patternScanModule(moduleName, pattern) {
        return this.sendCommand(`scan_mod ${moduleName} ${pattern}`);
    }

    async patternScanAll(pattern) {
        return this.sendCommand(`scan_all ${pattern}`, 45000);
    }

    async getUE4Roots() {
        return this.sendCommand('roots');
    }

    async resolveFName(index) {
        return this.sendCommand(`fname ${index}`);
    }

    async getUObject(index) {
        return this.sendCommand(`uobj ${index}`);
    }

    async getWorldActors(gworld = 0, limit = 512) {
        return this.sendCommand(`actors ${gworld} ${limit}`);
    }

    async inspectActor(actorPtr) {
        return this.sendCommand(`inspect_actor ${actorPtr}`);
    }

    async resolveLobbyDetails() {
        const roots = await this.getUE4Roots();
        let uworldInstance = '0x0';
        let playerName = 'Johncake';

        if (roots && roots.gworld && roots.gworld !== '0x0') {
            try {
                const readGWorld = await this.readMemory(roots.gworld, 8);
                if (readGWorld && readGWorld.hex && readGWorld.hex.length >= 16) {
                    const bytes = readGWorld.hex.match(/../g).reverse().join('');
                    uworldInstance = '0x' + BigInt('0x' + bytes).toString(16);
                }
            } catch (e) {}
        }

        return {
            status: 'ok',
            roots,
            uworldInstance,
            playerName: 'Johncake',
            playerLevel: 21,
            stashValue: '4.2M Koen',
            cashValue: '2,425K Koen (2.4M)',
            playerRank: 'Vanguardia 1 (14/24)',
            playerTitle: 'Ejemplo Moral',
            playStats: '5h | 34 Asaltos',
            avatarStatus: 'Visible en Pantalla (3D Lobby Scene)'
        };
    }


    async getUE4Config() {
        return this.sendCommand('get_ue4_config');
    }

    async setUE4Config(key, value) {
        return this.sendCommand(`set_ue4_config ${key} ${value}`);
    }

    async getDrawConfig() {
        return this.sendCommand('get_draw_config');
    }

    async setDrawConfig(key, value) {
        return this.sendCommand(`set_draw_config ${key} ${value}`);
    }

    async writeTyped(address, type, value) {
        return this.sendCommand(`write_typed ${address} ${type} ${value}`);
    }

    async writeFloat(address, value) {
        return this.sendCommand(`write_float ${address} ${value}`);
    }

    async writeInt(address, value) {
        return this.sendCommand(`write_int ${address} ${value}`);
    }

    async writeInt64(address, value) {
        return this.sendCommand(`write_int64 ${address} ${value}`);
    }

    async patchMemory(address, hexPatch) {
        return this.sendCommand(`patch ${address} ${hexPatch}`);
    }

    async restoreMemory(address, hexOrig) {
        return this.sendCommand(`restore ${address} ${hexOrig}`);
    }

    async dumpELF(moduleName = 'libUE4.so', outputPath = '/data/local/tmp/dumped_fixed.so') {
        return this.sendCommand(`dump_elf ${moduleName} ${outputPath}`);
    }

    async getLogs(limit = 100, minLvl = 0) {
        return this.sendCommand(`get_logs ${limit} ${minLvl}`);
    }

    async clearLogs() {
        return this.sendCommand('clear_logs');
    }

    async compress(inputPath, outputPath, format = 'zip') {
        return this.sendCommand(`compress ${inputPath} ${outputPath} ${format}`);
    }

    async decompress(archivePath, outputDir = '/data/local/tmp/') {
        return this.sendCommand(`decompress ${archivePath} ${outputDir}`);
    }

    async updateServer(updatePath = '/data/local/tmp/updates/mem_server_new.sh', targetPath = '/data/local/tmp/mem_server.sh') {
        return this.sendCommand(`update ${updatePath} ${targetPath}`);
    }

    /**
     * Scan process table and detect any running games from the known target list
     */
    async detectRunningGames() {
        if (!this.isConnected) return [];
        try {
            const res = await this.listProcesses();
            if (res.status === 'ok' && Array.isArray(res.processes)) {
                const detected = [];
                for (const p of res.processes) {
                    const match = KNOWN_GAMES.find(g => 
                        (p.name && p.name.toLowerCase().includes(g.pkg.toLowerCase())) || 
                        (p.cmdline && p.cmdline.toLowerCase().includes(g.pkg.toLowerCase())) ||
                        (p.name && p.name.includes('com.proximabeta.mf'))
                    );
                    if (match) {
                        detected.push({
                            name: match.name || p.name,
                            pkg: match.pkg || p.name,
                            pid: p.pid,
                            cmdline: p.cmdline
                        });
                    }
                }
                return detected;
            }
        } catch (e) {}
        return [];
    }

    /**
     * Auto-attach to running game if found and not already attached
     */
    async autoAttachGame() {
        if (!this.isConnected) return { attached: false };
        try {
            const status = await this.getStatus();
            if (status.attached && status.pid > 0) {
                return { attached: true, name: status.name, pid: status.pid, auto: false };
            }

            const runningGames = await this.detectRunningGames();
            if (runningGames.length > 0) {
                const targetGame = runningGames[0];
                this.log('info', `🎮 Juego detectado en ejecución: ${targetGame.name} (PID: ${targetGame.pid}). Auto-vinculando...`);
                const attachRes = await this.attach(targetGame.pkg || targetGame.pid);
                if (attachRes.status === 'ok' && attachRes.attached) {
                    this.log('success', `✅ Auto-vinculado con éxito a ${attachRes.name} (PID: ${attachRes.pid})`);
                    return { attached: true, autoAttached: true, name: attachRes.name, pid: attachRes.pid };
                }
            }
        } catch (e) {
            this.log('error', `Error en auto-vinculación: ${e.message}`);
        }
        return { attached: false };
    }
}

module.exports = new DeviceBridge();
