const express = require('express');
const http = require('http');
const path = require('path');
const fs = require('fs');
const cors = require('cors');
const WebSocket = require('ws');
const deviceBridge = require('./device_bridge');
const { McpServer, MCP_TOOLS } = require('./mcp_server');
const fileTransfer = require('./file_transfer');

const app = express();
const server = http.createServer(app);
const wss = new WebSocket.Server({ server });
const mcp = new McpServer();

const PORT = process.env.PORT || 3000;

app.use(cors());
app.use(express.json());
app.use(express.static(path.join(__dirname, '../public')));

// Broadcast to all connected WebSockets
function broadcast(data) {
    const msg = JSON.stringify(data);
    wss.clients.forEach(client => {
        if (client.readyState === WebSocket.OPEN) {
            client.send(msg);
        }
    });
}

// Hook deviceBridge events to WebSocket broadcast
deviceBridge.setLogCallback((logEntry) => {
    broadcast({ type: 'log', data: logEntry });
});

deviceBridge.setStatusCallback((connected) => {
    broadcast({ type: 'status_change', connected });
});

// ---------------- REST API ----------------

app.post('/api/adb-forward', async (req, res) => {
    const port = req.body.port || 8088;
    const result = await deviceBridge.runAdbForward(port);
    res.json(result);
});

app.post('/api/connect', async (req, res) => {
    const { host = '127.0.0.1', port = 8088, use_smart_usb = true } = req.body;
    try {
        let result;
        if (use_smart_usb) {
            result = await deviceBridge.smartConnect(host, port);
        } else {
            result = await deviceBridge.connect(host, port);
        }
        res.json({ ...result, mode: deviceBridge.connectionMode });
    } catch (err) {
        res.status(500).json({ success: false, error: err.message });
    }
});

app.get('/api/config/last_host', (req, res) => {
    const host = deviceBridge.getLastSavedHost() || '127.0.0.1';
    res.json({ host, port: 8088 });
});

app.get('/api/network/discover', async (req, res) => {
    try {
        const result = await deviceBridge.discoverDaemonOnNetwork();
        res.json(result);
    } catch (err) {
        res.status(500).json({ found: false, error: err.message });
    }
});

app.get('/api/usb/detect', async (req, res) => {
    try {
        const result = await deviceBridge.detectUsbDevices();
        res.json(result);
    } catch (err) {
        res.status(500).json({ hasUsb: false, error: err.message });
    }
});

app.post('/api/disconnect', (req, res) => {
    deviceBridge.disconnect();
    res.json({ success: true });
});

app.get('/api/status', async (req, res) => {
    if (!deviceBridge.isConnected) {
        return res.json({ connected: false, mode: deviceBridge.connectionMode, running_games: [] });
    }
    try {
        let status = await deviceBridge.getStatus();
        // If not attached, auto-check if a known game (like Arena Breakout Lite) is running
        if (!status.attached || status.pid <= 0) {
            const autoRes = await deviceBridge.autoAttachGame();
            if (autoRes.attached) {
                status = await deviceBridge.getStatus();
            }
        }
        const runningGames = await deviceBridge.detectRunningGames();
        res.json({ connected: true, mode: deviceBridge.connectionMode, ...status, running_games: runningGames });
    } catch (err) {
        res.json({ connected: false, mode: deviceBridge.connectionMode, error: err.message, running_games: [] });
    }
});

app.get('/api/games/detect', async (req, res) => {
    try {
        const running = await deviceBridge.detectRunningGames();
        const auto = await deviceBridge.autoAttachGame();
        res.json({ status: 'ok', running_games: running, auto_attached: auto });
    } catch (err) {
        res.status(500).json({ status: 'error', error: err.message });
    }
});

app.post('/api/attach', async (req, res) => {
    const target = req.body.target;
    if (!target) return res.status(400).json({ error: 'Target required' });
    try {
        const result = await deviceBridge.attach(target);
        res.json(result);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.post('/api/detach', async (req, res) => {
    try {
        const result = await deviceBridge.detach();
        res.json(result);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.get('/api/processes', async (req, res) => {
    try {
        const result = await deviceBridge.listProcesses();
        res.json(result);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.get('/api/modules', async (req, res) => {
    try {
        const result = await deviceBridge.getModules();
        res.json(result);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.post('/api/read', async (req, res) => {
    const { address, size } = req.body;
    if (!address) return res.status(400).json({ error: 'Address required' });
    try {
        const result = await deviceBridge.readMemory(address, size || 64);
        res.json(result);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.post('/api/write', async (req, res) => {
    const { address, hex_data } = req.body;
    if (!address || !hex_data) return res.status(400).json({ error: 'Address and hex_data required' });
    try {
        const result = await deviceBridge.writeMemory(address, hex_data);
        res.json(result);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.post('/api/write_typed', async (req, res) => {
    const { address, type = 'float', value } = req.body;
    if (!address || value === undefined) return res.status(400).json({ error: 'address and value required' });
    try {
        const result = await deviceBridge.writeTyped(address, type, value);
        res.json(result);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.post('/api/patch', async (req, res) => {
    const { address, hex_patch } = req.body;
    if (!address || !hex_patch) return res.status(400).json({ error: 'address and hex_patch required' });
    try {
        const result = await deviceBridge.patchMemory(address, hex_patch);
        res.json(result);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.post('/api/restore', async (req, res) => {
    const { address, hex_orig } = req.body;
    if (!address || !hex_orig) return res.status(400).json({ error: 'address and hex_orig required' });
    try {
        const result = await deviceBridge.restoreMemory(address, hex_orig);
        res.json(result);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.post('/api/read_ptr', async (req, res) => {
    const { base, offsets } = req.body;
    if (!base) return res.status(400).json({ error: 'Base address required' });
    try {
        const result = await deviceBridge.readPointerChain(base, offsets || []);
        res.json(result);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.post('/api/read_str', async (req, res) => {
    const { address, max_len } = req.body;
    if (!address) return res.status(400).json({ error: 'Address required' });
    try {
        const result = await deviceBridge.readString(address, max_len || 64);
        res.json(result);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.post('/api/scan', async (req, res) => {
    const { start, end, pattern } = req.body;
    if (!start || !end || !pattern) return res.status(400).json({ error: 'Start, end and pattern required' });
    try {
        const result = await deviceBridge.patternScan(start, end, pattern);
        res.json(result);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.post('/api/scan_mod', async (req, res) => {
    const { module: modName, pattern } = req.body;
    if (!modName || !pattern) return res.status(400).json({ error: 'Module name and pattern required' });
    try {
        const result = await deviceBridge.patternScanModule(modName, pattern);
        res.json(result);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.post('/api/scan_all', async (req, res) => {
    const { pattern } = req.body;
    if (!pattern) return res.status(400).json({ error: 'Pattern required' });
    try {
        const result = await deviceBridge.patternScanAll(pattern);
        res.json(result);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// UE4 Reflection Endpoints
app.get('/api/ue4/roots', async (req, res) => {
    try {
        const result = await deviceBridge.getUE4Roots();
        res.json(result);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.get('/api/ue4/fname/:index', async (req, res) => {
    const index = parseInt(req.params.index, 10);
    try {
        const result = await deviceBridge.resolveFName(index);
        res.json(result);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.get('/api/ue4/uobj/:index', async (req, res) => {
    const index = parseInt(req.params.index, 10);
    try {
        const result = await deviceBridge.getUObject(index);
        res.json(result);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.get('/api/ue4/actors', async (req, res) => {
    const gworld = req.query.gworld || 0;
    const limit = parseInt(req.query.limit, 10) || 512;
    try {
        const result = await deviceBridge.getWorldActors(gworld, limit);
        res.json(result);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.get('/api/ue4/actor/inspect', async (req, res) => {
    const actor = req.query.actor || req.query.ptr || req.query.address;
    if (!actor) return res.status(400).json({ error: 'actor pointer address required (e.g. ?actor=0x7430246000)' });
    try {
        const result = await deviceBridge.inspectActor(actor);
        res.json(result);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.post('/api/ue4/actor/inspect', async (req, res) => {
    const actor = req.body.actor || req.body.ptr || req.body.address;
    if (!actor) return res.status(400).json({ error: 'actor pointer address required' });
    try {
        const result = await deviceBridge.inspectActor(actor);
        res.json(result);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.get('/api/ue4/config', async (req, res) => {
    try {
        const result = await deviceBridge.getUE4Config();
        res.json(result);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.post('/api/ue4/config', async (req, res) => {
    const { key, value } = req.body;
    if (!key || value === undefined) {
        return res.status(400).json({ error: 'key and value required' });
    }
    try {
        const result = await deviceBridge.setUE4Config(key, value);
        res.json(result);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.get('/api/draw/config', async (req, res) => {
    try {
        const result = await deviceBridge.getDrawConfig();
        res.json(result);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.post('/api/draw/config', async (req, res) => {
    const { key, value } = req.body;
    if (!key || value === undefined) {
        return res.status(400).json({ error: 'key and value required' });
    }
    try {
        const result = await deviceBridge.setDrawConfig(key, value);
        res.json(result);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.get('/api/ue4/lobby', async (req, res) => {
    try {
        const result = await deviceBridge.resolveLobbyDetails();
        res.json(result);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.post('/api/ue4/dump_elf', async (req, res) => {
    const { module: modName = 'libUE4.so', output_path: outPath = '/data/local/tmp/dumped_fixed.so' } = req.body;
    try {
        const result = await deviceBridge.dumpELF(modName, outPath);
        res.json(result);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// Diagnostic & Logs Endpoints
app.get('/api/device/logs', async (req, res) => {
    const limit = parseInt(req.query.limit, 10) || 100;
    const minLvl = parseInt(req.query.min_level, 10) || 0;
    try {
        const result = await deviceBridge.getLogs(limit, minLvl);
        res.json(result);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.post('/api/device/clear_logs', async (req, res) => {
    try {
        const result = await deviceBridge.clearLogs();
        res.json(result);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// File Archive & Compression Endpoints
app.post('/api/device/compress', async (req, res) => {
    const { input_path, output_archive, format = 'zip' } = req.body;
    if (!input_path || !output_archive) {
        return res.status(400).json({ error: 'input_path and output_archive are required' });
    }
    try {
        const result = await deviceBridge.compress(input_path, output_archive, format);
        res.json(result);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.post('/api/device/decompress', async (req, res) => {
    const { archive_path, output_dir = '/data/local/tmp/' } = req.body;
    if (!archive_path) {
        return res.status(400).json({ error: 'archive_path is required' });
    }
    try {
        const result = await deviceBridge.decompress(archive_path, output_dir);
        res.json(result);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// High-Speed File Transfer Engine Endpoints

app.get('/api/fs/list', async (req, res) => {
    const dirPath = req.query.path || '/data/local/tmp/';
    try {
        const result = await fileTransfer.listDirectory(dirPath);
        res.json(result);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.get('/api/fs/stat', async (req, res) => {
    const filePath = req.query.path;
    if (!filePath) return res.status(400).json({ error: 'path is required' });
    try {
        const result = await fileTransfer.statFile(filePath);
        res.json(result);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.post('/api/fs/download', async (req, res) => {
    const { remote_path, local_filename } = req.body;
    if (!remote_path) return res.status(400).json({ error: 'remote_path is required' });

    const downloadsDir = path.join(__dirname, '../downloads');
    if (!fs.existsSync(downloadsDir)) fs.mkdirSync(downloadsDir, { recursive: true });

    const finalLocalPath = path.join(downloadsDir, local_filename || path.basename(remote_path));

    try {
        const result = await fileTransfer.downloadFile(remote_path, finalLocalPath, (prog) => {
            broadcastWs({ type: 'transfer_progress', ...prog, file: path.basename(remote_path) });
        });
        res.json({ ...result, local_path: finalLocalPath });
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.post('/api/fs/download_folder', async (req, res) => {
    const { remote_dir, local_filename } = req.body;
    if (!remote_dir) return res.status(400).json({ error: 'remote_dir is required' });

    const downloadsDir = path.join(__dirname, '../downloads');
    if (!fs.existsSync(downloadsDir)) fs.mkdirSync(downloadsDir, { recursive: true });

    const zipName = local_filename || `${path.basename(remote_dir) || 'folder'}_${Date.now()}.zip`;
    const finalLocalPath = path.join(downloadsDir, zipName);

    try {
        const result = await fileTransfer.downloadDirectory(remote_dir, finalLocalPath, (prog) => {
            broadcastWs({ type: 'transfer_progress', ...prog, file: zipName });
        });
        res.json({ ...result, local_path: finalLocalPath });
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.post('/api/fs/delete', async (req, res) => {
    const { path: targetPath, recursive = false } = req.body;
    if (!targetPath) return res.status(400).json({ error: 'path is required' });
    try {
        const result = await fileTransfer.deleteItem(targetPath, recursive);
        res.json(result);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.post('/api/fs/mkdir', async (req, res) => {
    const { path: dirPath } = req.body;
    if (!dirPath) return res.status(400).json({ error: 'path is required' });
    try {
        const result = await fileTransfer.makeDir(dirPath);
        res.json(result);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// Global Process Crash Prevention
process.on('uncaughtException', (err) => {
    console.error('⚠️ [SERVER CAUGHT EXCEPTION]:', err.message);
});

process.on('unhandledRejection', (reason, promise) => {
    console.error('⚠️ [SERVER UNHANDLED REJECTION]:', reason);
});

// Auto-Update & Push New Binary Endpoint
app.post('/api/device/push_update', async (req, res) => {
    const localBinary = path.join(__dirname, '../../android_server/libs/arm64-v8a/mem_server.sh');
    const updateDir = '/data/local/tmp/updates/';
    const updateRemoteFile = '/data/local/tmp/updates/mem_server_new.sh';
    const liveTargetFile = '/data/local/tmp/mem_server.sh';

    if (!fs.existsSync(localBinary)) {
        return res.status(404).json({ error: `Compiled binary not found at: ${localBinary}. Please run build.bat first.` });
    }

    try {
        await fileTransfer.makeDir(updateDir);
        const uploadRes = await fileTransfer.uploadFile(localBinary, updateRemoteFile, true, (prog) => {
            broadcast({ type: 'transfer_progress', ...prog, file: 'mem_server_new.sh (Update)' });
        });

        const updateRes = await deviceBridge.updateServer(updateRemoteFile, liveTargetFile);
        res.json({
            status: updateRes.status || 'ok',
            upload: uploadRes,
            update: updateRes,
            message: updateRes.message || 'Update applied successfully'
        });
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// Recompile C++ with NDK and Push Live Update in 1 Step
app.post('/api/device/rebuild_and_push', async (req, res) => {
    const buildBat = path.join(__dirname, '../../android_server/build.bat');
    const serverDir = path.join(__dirname, '../../android_server');

    const { exec } = require('child_process');
    exec('cmd /c "build.bat"', { cwd: serverDir }, async (err, stdout, stderr) => {
        if (err) {
            return res.status(500).json({ error: 'NDK Compilation failed: ' + (stderr || stdout || err.message) });
        }

        const localBinary = path.join(__dirname, '../../android_server/libs/arm64-v8a/mem_server.sh');
        if (!fs.existsSync(localBinary)) {
            return res.status(500).json({ error: 'Compiled binary not found after build' });
        }

        try {
            const updateDir = '/data/local/tmp/updates/';
            const updateRemoteFile = '/data/local/tmp/updates/mem_server_new.sh';
            const liveTargetFile = '/data/local/tmp/mem_server.sh';

            await fileTransfer.makeDir(updateDir);
            const uploadRes = await fileTransfer.uploadFile(localBinary, updateRemoteFile, true, (prog) => {
                broadcast({ type: 'transfer_progress', ...prog, file: 'mem_server.sh (NDK Build)' });
            });

            const updateRes = await deviceBridge.updateServer(updateRemoteFile, liveTargetFile);
            res.json({
                status: 'ok',
                build: stdout,
                upload: uploadRes,
                update: updateRes,
                message: 'Daemon recompilado y actualizado en el celular con éxito'
            });
        } catch (pushErr) {
            res.status(500).json({ error: 'Push failed: ' + pushErr.message });
        }
    });
});

// Client & Server Self-Reload / Reconnect
app.post('/api/server/reconnect', async (req, res) => {
    try {
        deviceBridge.disconnect();
        const connectRes = await deviceBridge.smartConnect(deviceBridge.host, deviceBridge.port);
        res.json({ status: 'ok', message: 'Bridge reconnected', result: connectRes });
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// System Version & Health Info
app.get('/api/system/info', (req, res) => {
    const localBinary = path.join(__dirname, '../../android_server/libs/arm64-v8a/mem_server.sh');
    let binaryMtime = null;
    let binarySize = 0;
    if (fs.existsSync(localBinary)) {
        const st = fs.statSync(localBinary);
        binaryMtime = st.mtime;
        binarySize = st.size;
    }
    res.json({
        client_version: '2.5.0-Turbo',
        node_version: process.version,
        binary_exists: fs.existsSync(localBinary),
        binary_mtime: binaryMtime,
        binary_size_bytes: binarySize,
        connected: deviceBridge.isConnected,
        connection_mode: deviceBridge.connectionMode,
        target_attached: deviceBridge.isAttached,
        target_pid: deviceBridge.targetPid
    });
});

// MCP JSON-RPC HTTP Endpoint
app.post('/mcp', async (req, res) => {
    try {
        const response = await mcp.handleJsonRpc(req.body);
        if (response) {
            res.json(response);
        } else {
            res.status(204).end();
        }
    } catch (err) {
        res.status(500).json({
            jsonrpc: '2.0',
            id: req.body?.id || null,
            error: { code: -32603, message: err.message }
        });
    }
});

app.get('/api/mcp/tools', (req, res) => {
    res.json({ tools: MCP_TOOLS });
});

// WebSocket Connection handling
wss.on('connection', (ws) => {
    ws.send(JSON.stringify({
        type: 'init',
        connected: deviceBridge.isConnected,
        host: deviceBridge.host,
        port: deviceBridge.port
    }));
});

server.listen(PORT, () => {
    console.log(`=======================================================`);
    console.log(`   [+] LINX_MCP_LECTOR - PC CLIENT & MCP SERVER        `);
    console.log(`   [+] Web UI: http://localhost:${PORT}                `);
    console.log(`   [+] MCP Server Endpoint: http://localhost:${PORT}/mcp `);
    console.log(`   [+] Powered by: uam.lol/j                           `);
    console.log(`-------------------------------------------------------`);
    console.log(`   [🤖 CONFIGURACIÓN MCP PARA ANTIGRAVITY / CLAUDE]:   `);
    console.log(`   Añade esta entrada dentro de "mcpServers": { ... } en tu mcp_config.json:`);
    console.log(`    "linx-memory": {`);
    console.log(`      "command": "node",`);
    console.log(`      "args": [`);
    console.log(`        "${path.join(__dirname, 'mcp_server.js').replace(/\\/g, '\\\\')}"`);
    console.log(`      ]`);
    console.log(`    }`);
    console.log(`=======================================================`);
});
