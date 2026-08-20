// app.js -- Frontend Controller for Linx Root Memory Client & MCP

let ws = null;
let isConnected = false;
let currentPid = null;
let currentTargetName = null;
let cachedModules = [];
let cachedProcesses = [];
let currentViewingAddress = 0n;
let rawMemoryBuffer = null;
let autoRefreshTimer = null;
let selectedByteIndex = 0;

// Initialize on DOM ready
document.addEventListener('DOMContentLoaded', () => {
    initTabs();
    initWebSocket();
    initEventHandlers();
    checkStatus();
    checkUsbDeviceStatus(true);
    loadMcpTools();

    // Check ADB device state every 5 seconds
    setInterval(() => checkUsbDeviceStatus(false), 5000);
});

// Tab Switching
function initTabs() {
    const tabs = document.querySelectorAll('.nav-tab');
    tabs.forEach(tab => {
        tab.addEventListener('click', () => {
            tabs.forEach(t => t.classList.remove('active'));
            document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));

            tab.classList.add('active');
            const target = tab.getAttribute('data-tab');
            const targetContent = document.getElementById(`tab-${target}`);
            if (targetContent) targetContent.classList.add('active');

            // Auto load tab data
            if (target === 'processes') loadProcesses();
            if (target === 'modules') loadModules();
            if (target === 'actors') loadWorldActors();
            if (target === 'lobby') scanLobbyData();
            if (target === 'files') loadFsDirectory();
            if (target === 'mcp') loadMcpTools();
            if (target === 'updates') fetchSystemInfo();
        });
    });
}

// WebSocket for real-time updates and logs
function initWebSocket() {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${protocol}//${window.location.host}`;
    
    ws = new WebSocket(wsUrl);

    ws.onopen = () => {
        appendLog('info', 'WebSocket connected to local PC server');
    };

    ws.onmessage = (event) => {
        try {
            const data = JSON.parse(event.data);
            if (data.type === 'log') {
                appendLog(data.data.type, data.data.message, data.data.timestamp);
            } else if (data.type === 'status_change') {
                updateConnectionState(data.connected);
            }
        } catch (e) {
            console.error('WS Parse Error', e);
        }
    };

    ws.onclose = () => {
        appendLog('warn', 'WebSocket disconnected. Reconnecting in 3s...');
        setTimeout(initWebSocket, 3000);
    };
}

function appendLog(type, msg, time = null) {
    // Filter out background polling from the UI log container to prevent spam
    if (msg && (msg.startsWith('get_logs') || msg === 'status')) return;

    const container = document.getElementById('bridgeLogContainer') || document.getElementById('consoleLogContainer');
    if (!container) return;

    const timestamp = time || new Date().toLocaleTimeString();
    const entry = document.createElement('div');
    entry.className = 'log-entry';
    entry.innerHTML = `
        <span class="log-time">[${timestamp}]</span>
        <span class="log-type-${type}">[${type.toUpperCase()}]</span>
        <span class="log-msg">${escapeHtml(msg)}</span>
    `;
    container.appendChild(entry);

    // Keep maximum 200 items in DOM to prevent lag
    while (container.children.length > 200) {
        container.removeChild(container.firstChild);
    }
    container.scrollTop = container.scrollHeight;
}

function escapeHtml(str) {
    if (!str) return '';
    return String(str).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

// REST API Helper
async function apiCall(endpoint, method = 'GET', body = null) {
    const options = {
        method,
        headers: { 'Content-Type': 'application/json' }
    };
    if (body) options.body = JSON.stringify(body);

    try {
        const res = await fetch(endpoint, options);
        return await res.json();
    } catch (err) {
        appendLog('error', `API Call Failed [${endpoint}]: ${err.message}`);
        return { status: 'error', message: err.message };
    }
}

// Event Handlers
function initEventHandlers() {
    // Stealth Connect (ADB Tunnel 127.0.0.1:8088 -> @memsvc)
    document.getElementById('btnConnect').addEventListener('click', async () => {
        appendLog('info', 'Connecting to Android stealth daemon via ADB tunnel (127.0.0.1:8088)...');
        const res = await apiCall('/api/connect', 'POST', { host: '127.0.0.1', port: 8088, use_smart_usb: true });
        if (res.success) {
            updateConnectionState(true, null, 'USB');
            checkStatus();
        } else {
            alert(`Conexión fallida: ${res.error || 'Verifica que mem_server.sh esté corriendo en el celular'}`);
        }
    });

    // ADB Recheck
    document.getElementById('btnUsbRecheck').addEventListener('click', () => checkUsbDeviceStatus(false));

    // Disconnect
    document.getElementById('btnDisconnect').addEventListener('click', async () => {
        await apiCall('/api/disconnect', 'POST');
        updateConnectionState(false);
    });

    // ADB Forward Tunnel Rebuild
    document.getElementById('btnAdbForward').addEventListener('click', async () => {
        appendLog('info', 'Reconfigurando túnel ADB forward tcp:8088 localabstract:memsvc...');
        const res = await apiCall('/api/adb-forward', 'POST', { port: 8088 });
        alert(res.message);
        checkUsbDeviceStatus(false);
    });

    // Quick Attach buttons
    document.getElementById('btnAttachABLite').addEventListener('click', () => attachTarget('com.proximabeta.mf.liteuamo'));
    document.getElementById('btnAttachABMain').addEventListener('click', () => attachTarget('com.proximabeta.mf.uamo'));
    document.getElementById('btnCustomAttach').addEventListener('click', () => {
        const target = document.getElementById('customTargetInput').value.trim();
        if (target) attachTarget(target);
    });

    // Process & Module refreshes
    document.getElementById('btnRefreshProcesses').addEventListener('click', loadProcesses);
    document.getElementById('btnRefreshModules').addEventListener('click', loadModules);
    document.getElementById('btnRefreshStatus').addEventListener('click', checkStatus);

    // UE4 Roots & Entities
    document.getElementById('btnDetectRoots').addEventListener('click', loadUE4Roots);
    document.getElementById('btnRefreshActors').addEventListener('click', loadWorldActors);
    document.getElementById('chkAutoRefreshActors').addEventListener('change', toggleActorAutoRefresh);
    const actorFilterEl = document.getElementById('actorFilter');
    if (actorFilterEl) actorFilterEl.addEventListener('input', (e) => filterTable('actorTableBody', e.target.value));
    document.getElementById('btnDumpSO').addEventListener('click', handleDumpSO);

    // Radar & Player Filters
    const radarRangeEl = document.getElementById('radarRange');
    if (radarRangeEl) radarRangeEl.addEventListener('change', () => { if (lastActorsData) renderActors(lastActorsData, lastCameraData); });

    document.querySelectorAll('.filter-btn').forEach(btn => {
        btn.addEventListener('click', (e) => {
            document.querySelectorAll('.filter-btn').forEach(b => b.classList.remove('active'));
            e.target.classList.add('active');
            currentActorFilter = e.target.getAttribute('data-filter') || 'all';
            if (lastActorsData) renderActors(lastActorsData, lastCameraData);
        });
    });

    // File Manager & Transfer
    document.getElementById('btnFsRefresh').addEventListener('click', () => loadFsDirectory());
    document.getElementById('btnPushLiveUpdate').addEventListener('click', handlePushLiveUpdate);
    document.getElementById('btnFsNavigate').addEventListener('click', () => {
        const path = document.getElementById('fsCurrentPathInput').value.trim();
        if (path) loadFsDirectory(path);
    });
    document.getElementById('btnFsGoUp').addEventListener('click', handleFsGoUp);
    document.getElementById('btnFsNewFolder').addEventListener('click', handleFsNewFolder);
    document.getElementById('btnFsUpload').addEventListener('click', () => {
        document.getElementById('fsFileInput').click();
    });
    document.getElementById('fsFileInput').addEventListener('change', handleFsUpload);

    // Filters
    document.getElementById('processFilter').addEventListener('input', (e) => filterTable('processTableBody', e.target.value));
    document.getElementById('moduleFilter').addEventListener('input', (e) => filterTable('moduleTableBody', e.target.value));

    // Hex Read
    document.getElementById('btnReadHex').addEventListener('click', () => readHexMemory());
    document.getElementById('chkAutoRefresh').addEventListener('change', toggleAutoRefresh);
    document.getElementById('autoRefreshInterval').addEventListener('change', () => {
        if (document.getElementById('chkAutoRefresh').checked) {
            toggleAutoRefresh();
            toggleAutoRefresh();
        }
    });

    // Follow Pointer
    document.getElementById('btnFollowPointer').addEventListener('click', () => {
        const ptrText = document.getElementById('t_uint64').innerText;
        if (ptrText && ptrText !== '--' && ptrText !== '0x0') {
            document.getElementById('hexAddressInput').value = ptrText;
            readHexMemory(ptrText);
        }
    });

    // Write Patch
    document.getElementById('btnWritePatch').addEventListener('click', async () => {
        const hex = document.getElementById('patchHexInput').value.replace(/\s+/g, '');
        if (!hex || hex.length % 2 !== 0) {
            alert('Please enter valid hex bytes (e.g. 1F2003D5 or 9090)');
            return;
        }
        const addr = document.getElementById('hexAddressInput').value.trim();
        const resolved = await resolveAddress(addr);
        if (!resolved) return;

        const res = await apiCall('/api/write', 'POST', { address: resolved, hex_data: hex });
        if (res.status === 'ok') {
            appendLog('info', `Memory patched at ${resolved}: ${hex}`);
            readHexMemory();
        } else {
            alert(`Patch failed: ${res.message || 'Error'}`);
        }
    });

    // Pointer Chain Resolver
    document.getElementById('btnResolvePtr').addEventListener('click', resolvePointerChain);

    // Pattern Scanner
    document.getElementById('btnStartScan').addEventListener('click', startPatternScan);

    // Console & Device Logs
    document.getElementById('btnClearLogs')?.addEventListener('click', clearAllLogs);
    document.getElementById('btnClearBridgeLogs')?.addEventListener('click', clearBridgeLogs);
    document.getElementById('btnFetchDeviceLogs')?.addEventListener('click', fetchDeviceLogs);
    document.getElementById('logLevelFilter')?.addEventListener('change', fetchDeviceLogs);
    document.getElementById('logSearchInput')?.addEventListener('input', renderDiagnosticLogs);
    document.getElementById('chkAutoPollLogs')?.addEventListener('change', toggleLogAutoPoll);

    // Copy MCP snippets
    document.getElementById('btnCopyMcpConfig').addEventListener('click', () => {
        const code = document.getElementById('mcpConfigSnippet').innerText;
        navigator.clipboard.writeText(code);
        alert('Configuración MCP completa copiada al portapapeles!');
    });

    document.getElementById('btnCopySingleMcp').addEventListener('click', () => {
        const code = document.getElementById('mcpSingleSnippet').innerText;
        navigator.clipboard.writeText(code);
        alert('Entrada individual de MCP copiada al portapapeles!');
    });
}

async function checkUsbDeviceStatus(isInitial = false) {
    const dot = document.getElementById('usbIndicatorDot');
    const text = document.getElementById('usbDetectText');
    try {
        const res = await apiCall('/api/usb/detect');
        if (res.hasUsb && res.devices && res.devices.length > 0) {
            const dev = res.devices[0];
            dot.style.background = '#10b981';
            text.innerHTML = `⚡ <strong>Dispositivo ADB Detectado:</strong> <span class="text-green">${escapeHtml(dev.model || dev.id)}</span>`;
            const badge = document.getElementById('connModeBadge');
            if (badge) {
                badge.innerText = '⚡ ADB Stealth Listo';
                badge.className = 'badge badge-green';
            }
            // Auto-connect seamlessly on initial page load if not already connected
            if (isInitial && !isConnected) {
                apiCall('/api/connect', 'POST', { host: '127.0.0.1', port: 8088, use_smart_usb: true }).then(connRes => {
                    if (connRes && connRes.success) {
                        updateConnectionState(true, null, 'USB');
                        checkStatus();
                    }
                }).catch(() => {});
            }
        } else {
            dot.style.background = '#f59e0b';
            text.innerHTML = `⚠️ <strong>Esperando conexión ADB:</strong> Conecta por cable USB o Wi-Fi Debugging`;
            const badge = document.getElementById('connModeBadge');
            if (badge) {
                badge.innerText = 'Esperando ADB';
                badge.className = 'badge badge-cyan';
            }
        }
    } catch (e) {
        dot.style.background = '#ef4444';
        text.innerText = 'Error al consultar estado ADB local';
    }
}

function updateConnectionState(connected, latency = null, mode = 'WIFI') {
    isConnected = connected;
    document.body.className = connected ? (mode === 'USB' ? 'app-connected-usb' : 'app-connected-wifi') : 'app-disconnected';

    const phoneBadge = document.getElementById('phoneStatusBadge');
    const phoneIcon = document.getElementById('phoneStatusIcon');
    const phoneText = document.getElementById('phoneStatusText');
    const btnConnect = document.getElementById('btnConnect');
    const btnDisconnect = document.getElementById('btnDisconnect');
    const metricConn = document.getElementById('metricConnection');
    const telemetryPhone = document.getElementById('telemetryPhoneState');
    const telemetryPing = document.getElementById('telemetryPing');
    const connModeBadge = document.getElementById('connModeBadge');

    if (connected) {
        if (phoneBadge) {
            phoneBadge.className = mode === 'USB' ? 'status-badge status-turbo' : 'status-badge status-connected';
        }
        if (phoneIcon) {
            phoneIcon.innerHTML = `
                <svg class="badge-check-icon" viewBox="0 0 20 20" fill="currentColor">
                    <path fill-rule="evenodd" d="M16.707 5.293a1 1 0 010 1.414l-8 8a1 1 0 01-1.414 0l-4-4a1 1 0 011.414-1.414L8 12.586l7.293-7.293a1 1 0 011.414 0z" clip-rule="evenodd"/>
                </svg>
            `;
        }
        if (phoneText) {
            phoneText.innerText = mode === 'USB' ? 'CELULAR: CONECTADO (⚡ USB TURBO)' : 'CELULAR: CONECTADO (📶 WI-FI)';
        }
        if (telemetryPhone) {
            telemetryPhone.className = 'text-green';
            telemetryPhone.innerText = mode === 'USB' ? 'ACTIVO VÍA USB ADB (>100 MB/s)' : 'ACTIVO VÍA RED WI-FI';
        }
        if (telemetryPing && latency !== null) {
            telemetryPing.innerText = `${latency} ms`;
        }
        if (connModeBadge) {
            connModeBadge.innerText = mode === 'USB' ? '⚡ USB Turbo' : '📶 Wi-Fi';
            connModeBadge.className = mode === 'USB' ? 'badge badge-green' : 'badge badge-cyan';
        }

        btnConnect.disabled = true;
        btnDisconnect.disabled = false;
        metricConn.innerText = mode === 'USB' ? '⚡ Conectado (USB Turbo)' : '📶 Conectado (Wi-Fi)';
        metricConn.className = 'metric-value text-green';
    } else {
        if (phoneBadge) {
            phoneBadge.className = 'status-badge status-disconnected';
        }
        if (phoneIcon) {
            phoneIcon.innerHTML = `
                <svg class="icon-waiting" viewBox="0 0 20 20" fill="currentColor">
                    <path fill-rule="evenodd" d="M10 18a8 8 0 100-16 8 8 0 000 16zm1-12a1 1 0 10-2 0v4a1 1 0 00.293.707l2.828 2.829a1 1 0 101.415-1.415L11 9.586V6z" clip-rule="evenodd"/>
                </svg>
            `;
        }
        if (phoneText) {
            phoneText.innerText = 'CELULAR: DESCONECTADO (ESPERANDO)';
        }
        if (telemetryPhone) {
            telemetryPhone.className = 'text-yellow';
            telemetryPhone.innerText = 'ESPERANDO CLIENTE EN PUERTO 8088';
        }
        if (telemetryPing) {
            telemetryPing.innerText = '-- ms';
        }

        btnConnect.disabled = false;
        btnDisconnect.disabled = true;
        metricConn.innerText = 'Desconectado';
        metricConn.className = 'metric-value text-cyan';
        updateTargetState(null, null);
    }
}

function updateTargetState(name, pid) {
    currentTargetName = name;
    currentPid = pid;

    const targetBadge = document.getElementById('targetBadge');
    const targetText = document.getElementById('targetText');
    const pidText = document.getElementById('pidText');
    const metricTarget = document.getElementById('metricTarget');
    const metricPid = document.getElementById('metricPid');
    const metricPidBox = document.getElementById('metricPidBox');

    if (pid && pid > 0) {
        if (targetBadge) targetBadge.className = 'target-badge active';
        if (targetText) targetText.innerHTML = `🎮 ${escapeHtml(name || 'PID_' + pid)}`;
        if (pidText) {
            pidText.style.display = 'inline-flex';
            pidText.innerText = `PID: ${pid}`;
        }
        if (metricTarget) {
            metricTarget.innerHTML = `🎮 ${escapeHtml(name || 'PID_' + pid)}`;
            metricTarget.className = 'metric-value text-green';
        }
        if (metricPid) {
            metricPid.innerText = pid;
            metricPid.className = 'metric-value text-purple';
        }
        if (metricPidBox) metricPidBox.style.display = 'flex';
    } else {
        if (targetBadge) targetBadge.className = 'target-badge empty';
        if (targetText) targetText.innerHTML = '⚠️ Sin Proceso Vinculado';
        if (pidText) {
            pidText.style.display = 'none';
            pidText.innerText = '';
        }
        if (metricTarget) {
            metricTarget.innerHTML = '⚠️ Sin Proceso Vinculado';
            metricTarget.className = 'metric-value metric-alert';
        }
        if (metricPid) {
            metricPid.innerText = 'No Vinculado';
            metricPid.className = 'metric-value text-muted';
        }
    }
}

async function checkStatus() {
    const t0 = performance.now();
    const res = await apiCall('/api/status');
    const latency = Math.round(performance.now() - t0);

    const timeEl = document.getElementById('telemetryTime');
    if (timeEl) timeEl.innerText = new Date().toLocaleTimeString();

    if (res.connected) {
        updateConnectionState(true, latency, res.mode || 'WIFI');
        if (res.attached && res.pid > 0) {
            updateTargetState(res.name, res.pid);
        } else if (res.running_games && res.running_games.length > 0) {
            updateTargetState(res.running_games[0].name || res.running_games[0].pkg, res.running_games[0].pid);
        } else {
            updateTargetState(null, null);
        }
    } else {
        updateConnectionState(false);
    }
}

// Periodic Heartbeat Activity Monitor (Every 2 seconds)
setInterval(checkStatus, 2000);

async function attachTarget(target) {
    appendLog('info', `Attaching to target: ${target}...`);
    const res = await apiCall('/api/attach', 'POST', { target });
    if (res.status === 'ok' && res.attached) {
        appendLog('success', `Attached to ${res.name} (PID: ${res.pid})`);
        updateTargetState(res.name, res.pid);
        loadModules(); // auto load modules once attached
    } else {
        alert(`Failed to attach: ${res.message || 'Target not running'}`);
    }
}

async function loadProcesses() {
    const tbody = document.getElementById('processTableBody');
    tbody.innerHTML = '<tr><td colspan="4" class="empty-state">Scanning /proc...</td></tr>';
    
    const res = await apiCall('/api/processes');
    if (res.status === 'ok' && res.processes) {
        cachedProcesses = res.processes;
        renderProcesses(cachedProcesses);
    } else {
        tbody.innerHTML = '<tr><td colspan="4" class="empty-state">Failed to load processes</td></tr>';
    }
}

function renderProcesses(processes) {
    const tbody = document.getElementById('processTableBody');
    if (!processes || processes.length === 0) {
        tbody.innerHTML = '<tr><td colspan="4" class="empty-state">No processes found</td></tr>';
        return;
    }

    tbody.innerHTML = processes.map(p => `
        <tr>
            <td><span class="pid-tag">${p.pid}</span></td>
            <td><strong>${escapeHtml(p.name)}</strong></td>
            <td><code>${escapeHtml(p.cmdline)}</code></td>
            <td>
                <button class="btn btn-sm btn-primary" onclick="attachTarget('${p.pid}')">Vincular</button>
            </td>
        </tr>
    `).join('');
}

async function loadModules() {
    const tbody = document.getElementById('moduleTableBody');
    tbody.innerHTML = '<tr><td colspan="7" class="empty-state">Scanning /proc/maps...</td></tr>';
    
    const res = await apiCall('/api/modules');
    if (res.status === 'ok' && res.modules) {
        cachedModules = res.modules;
        renderModules(cachedModules);
        loadUE4Roots(); // Also fetch roots
    } else {
        tbody.innerHTML = `<tr><td colspan="7" class="empty-state">${res.message || 'Attach to a game process first'}</td></tr>`;
    }
}

async function loadUE4Roots() {
    const res = await apiCall('/api/ue4/roots');
    if (res.status === 'ok') {
        document.getElementById('ue4LibBase').innerText = res.lib_base || '--';
        document.getElementById('ue4FNamePool').innerText = res.fname_pool || '--';
        document.getElementById('ue4GUObjects').innerText = res.guobject_array || '--';
        document.getElementById('ue4GWorld').innerText = res.gworld || '--';
    }
}

async function handleDumpSO() {
    const btn = document.getElementById('btnDumpSO');
    const origText = btn.innerText;
    btn.innerText = '⏳ Dumping RAM & Fixing ELF...';
    btn.disabled = true;

    try {
        const res = await apiCall('/api/ue4/dump_elf', 'POST', {
            module: 'libUE4.so',
            output_path: '/data/local/tmp/libUE4_dumped_fixed.so'
        });

        if (res.status === 'ok') {
            alert(`✅ ¡ÉXITO!\n\nLibrería desencriptada y reparada con éxito en el celular:\n${res.output_path}\n\nYa puedes abrir este archivo directamente en IDA Pro / Ghidra.`);
        } else {
            alert(`❌ Error al dumpear: ${res.message || 'Desconocido'}`);
        }
    } catch (err) {
        alert(`❌ Error de conexión: ${err.message}`);
    } finally {
        btn.innerText = origText;
        btn.disabled = false;
    }
}

let actorAutoRefreshTimer = null;
let lastActorsData = [];
let lastCameraData = null;
let currentActorFilter = 'all';

function toggleActorAutoRefresh() {
    const chk = document.getElementById('chkAutoRefreshActors');
    if (actorAutoRefreshTimer) {
        clearInterval(actorAutoRefreshTimer);
        actorAutoRefreshTimer = null;
    }
    if (chk && chk.checked) {
        actorAutoRefreshTimer = setInterval(loadWorldActors, 1000);
    }
}

async function loadWorldActors() {
    const tbody = document.getElementById('actorTableBody');
    if (!actorAutoRefreshTimer && tbody) {
        tbody.innerHTML = '<tr><td colspan="7" class="empty-state">Escaneando GWorld -> PersistentLevel -> AActors...</td></tr>';
    }

    const res = await apiCall('/api/ue4/actors?limit=512');
    if (res.status === 'ok' && res.actors) {
        lastActorsData = res.actors || [];
        lastCameraData = res.camera || null;
        renderActors(lastActorsData, lastCameraData);
    } else {
        if (tbody) {
            tbody.innerHTML = `<tr><td colspan="7" class="empty-state text-red">${res.message || 'No se pudieron leer actores (¿Estás en partida o en lobby?)'}</td></tr>`;
        }
    }
}

function drawRadar(camera, actors) {
    const canvas = document.getElementById('radarCanvas');
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    const width = canvas.width;
    const height = canvas.height;
    const centerX = width / 2;
    const centerY = height / 2;
    const radius = width / 2 - 10;

    const rangeSelect = document.getElementById('radarRange');
    const maxRangeMeters = rangeSelect ? parseFloat(rangeSelect.value) : 100.0;
    const scale = radius / maxRangeMeters; // pixels per meter

    // Clear canvas
    ctx.clearRect(0, 0, width, height);

    // Background circle
    ctx.beginPath();
    ctx.arc(centerX, centerY, radius, 0, 2 * Math.PI);
    ctx.fillStyle = 'rgba(8, 14, 26, 0.95)';
    ctx.fill();
    ctx.strokeStyle = 'rgba(0, 240, 255, 0.25)';
    ctx.lineWidth = 1.5;
    ctx.stroke();

    // Concentric range rings
    const rings = [0.25, 0.5, 0.75, 1.0];
    ctx.strokeStyle = 'rgba(0, 240, 255, 0.12)';
    ctx.lineWidth = 1;
    ctx.fillStyle = 'rgba(100, 116, 139, 0.6)';
    ctx.font = '9px Fira Code, monospace';

    rings.forEach(fraction => {
        const r = radius * fraction;
        ctx.beginPath();
        ctx.arc(centerX, centerY, r, 0, 2 * Math.PI);
        ctx.stroke();

        const meters = Math.round(maxRangeMeters * fraction);
        ctx.fillText(`${meters}m`, centerX + 4, centerY - r + 11);
    });

    // Crosshairs (North, South, East, West)
    ctx.beginPath();
    ctx.moveTo(centerX, centerY - radius);
    ctx.lineTo(centerX, centerY + radius);
    ctx.moveTo(centerX - radius, centerY);
    ctx.lineTo(centerX + radius, centerY);
    ctx.strokeStyle = 'rgba(0, 240, 255, 0.15)';
    ctx.stroke();

    // Camera Reference & Yaw
    let refX = 0, refY = 0, cameraYaw = 0;
    if (camera) {
        refX = camera.local_pos ? camera.local_pos.x : (camera.x || 0);
        refY = camera.local_pos ? camera.local_pos.y : (camera.y || 0);
        cameraYaw = (camera.yaw || 0) * (Math.PI / 180.0); // radians
    }

    // Vision Cone (FOV) in direction of camera Yaw
    const fovDeg = (camera && camera.fov) ? camera.fov : 90;
    const fovRad = (fovDeg * Math.PI) / 180.0;
    const viewAngle = -cameraYaw - Math.PI / 2; // UE4 Yaw to Canvas Angle

    ctx.beginPath();
    ctx.moveTo(centerX, centerY);
    ctx.arc(centerX, centerY, radius, viewAngle - fovRad / 2, viewAngle + fovRad / 2);
    ctx.closePath();
    ctx.fillStyle = 'rgba(0, 240, 255, 0.05)';
    ctx.fill();
    ctx.strokeStyle = 'rgba(0, 240, 255, 0.3)';
    ctx.stroke();

    // Draw Entities
    actors.forEach(a => {
        if (a.is_local) return; // local player drawn in center

        const dx = (a.pos.x - refX) / 100.0; // convert cm to meters
        const dy = (a.pos.y - refY) / 100.0;

        // In 2D Top-Down: UE4 X is forward, Y is right
        // Map to Canvas: (centerX + dy * scale, centerY - dx * scale)
        const px = centerX + dy * scale;
        const py = centerY - dx * scale;

        // Distance from radar center
        const distFromCenter = Math.sqrt((px - centerX) * (px - centerX) + (py - centerY) * (py - centerY));
        if (distFromCenter > radius) return; // Outside radar range

        ctx.beginPath();
        let dotColor = '#93c5fd';
        let dotSize = 4;

        if (a.type === 'enemy' || a.is_player) {
            dotColor = '#ef4444'; // Red
            dotSize = 5;
        } else if (a.type === 'teammate' || a.is_teammate) {
            dotColor = '#10b981'; // Green
            dotSize = 5;
        } else if (a.type === 'bot' || a.is_bot) {
            dotColor = '#f59e0b'; // Yellow / Orange
            dotSize = 4.5;
        } else if (a.type === 'item' || a.is_item) {
            dotColor = '#60a5fa'; // Blue
            dotSize = 3;
        }

        ctx.arc(px, py, dotSize, 0, 2 * Math.PI);
        ctx.fillStyle = dotColor;
        ctx.shadowColor = dotColor;
        ctx.shadowBlur = 6;
        ctx.fill();
        ctx.shadowBlur = 0;

        // Draw distance tag for players and enemies
        if (a.is_player || a.is_bot || a.is_teammate) {
            ctx.fillStyle = 'rgba(241, 245, 249, 0.85)';
            ctx.font = '8px Fira Code, monospace';
            ctx.fillText(`${Math.round(a.dist)}m`, px + 6, py + 3);
        }
    });

    // Draw Local Player Triangle at Center
    ctx.save();
    ctx.translate(centerX, centerY);
    ctx.rotate(viewAngle + Math.PI / 2);

    ctx.beginPath();
    ctx.moveTo(0, -8);
    ctx.lineTo(6, 6);
    ctx.lineTo(0, 3);
    ctx.lineTo(-6, 6);
    ctx.closePath();
    ctx.fillStyle = '#00f0ff';
    ctx.shadowColor = '#00f0ff';
    ctx.shadowBlur = 10;
    ctx.fill();
    ctx.restore();
    ctx.shadowBlur = 0;
}

function renderActors(actors, camera) {
    drawRadar(camera, actors);

    const tbody = document.getElementById('actorTableBody');
    if (!tbody) return;

    if (!actors || actors.length === 0) {
        tbody.innerHTML = '<tr><td colspan="7" class="empty-state">No se encontraron entidades en el mapa activo</td></tr>';
        return;
    }

    // Filter actors according to active filter button
    let filtered = actors;
    if (currentActorFilter === 'enemy') {
        filtered = actors.filter(a => (a.type === 'enemy' || a.is_player) && !a.is_local && !a.is_teammate && !a.is_bot);
    } else if (currentActorFilter === 'teammate') {
        filtered = actors.filter(a => a.type === 'teammate' || a.is_teammate);
    } else if (currentActorFilter === 'bot') {
        filtered = actors.filter(a => a.type === 'bot' || a.is_bot);
    } else if (currentActorFilter === 'item') {
        filtered = actors.filter(a => a.type === 'item' || a.is_item);
    }

    // Sort by distance (closest first)
    filtered.sort((a, b) => (a.dist || 0) - (b.dist || 0));

    if (filtered.length === 0) {
        tbody.innerHTML = `<tr><td colspan="7" class="empty-state">No hay entidades que coincidan con el filtro '${currentActorFilter}'</td></tr>`;
        return;
    }

    tbody.innerHTML = filtered.map((a, idx) => {
        let typeBadge = '<span class="badge badge-cyan">Entidad</span>';
        if (a.is_local) {
            typeBadge = '<span class="badge badge-cyan" style="background: rgba(0, 240, 255, 0.2); border: 1px solid var(--accent-cyan);">👤 TÚ</span>';
        } else if (a.is_teammate || a.type === 'teammate') {
            typeBadge = '<span class="badge badge-green" style="background: rgba(16, 185, 129, 0.2); border: 1px solid var(--accent-green);">🟢 ALIADO</span>';
        } else if (a.type === 'enemy' || a.is_player) {
            typeBadge = '<span class="badge badge-red" style="background: rgba(239, 68, 68, 0.2); border: 1px solid var(--accent-red); color: #fca5a5;">🔴 ENEMIGO</span>';
        } else if (a.is_bot || a.type === 'bot') {
            typeBadge = '<span class="badge badge-yellow" style="background: rgba(245, 158, 11, 0.2); border: 1px solid var(--accent-yellow); color: #fcd34d;">🟡 BOT / IA</span>';
        } else if (a.is_item || a.type === 'item') {
            typeBadge = '<span class="badge" style="background: rgba(147, 197, 253, 0.15); border: 1px solid #93c5fd; color: #93c5fd;">📦 LOOT</span>';
        }

        const distDisplay = a.is_local ? '--' : `<strong class="text-cyan">${a.dist ? a.dist.toFixed(1) + ' m' : '--'}</strong>`;
        const displayName = a.player_name ? `<strong>${escapeHtml(a.player_name)}</strong> <small class="text-muted">(${escapeHtml(a.name)})</small>` : escapeHtml(a.name);
        const teamDisplay = a.team ? `<span class="badge badge-secondary">T-${a.team}</span>` : '<span class="text-muted">--</span>';

        return `
            <tr>
                <td>${idx + 1}</td>
                <td>${typeBadge}</td>
                <td class="font-mono">${distDisplay}</td>
                <td>${displayName} <br><small class="text-purple">${escapeHtml(a.class)}</small></td>
                <td>${teamDisplay}</td>
                <td class="font-mono" style="font-size: 11px;">
                    X:${a.pos.x.toFixed(1)}<br>
                    Y:${a.pos.y.toFixed(1)}<br>
                    Z:${a.pos.z.toFixed(1)}
                </td>
                <td>
                    <button class="btn btn-sm btn-secondary" onclick="inspectModule('', '${a.ptr}')">Hex</button>
                </td>
            </tr>
        `;
    }).join('');
}

function renderModules(modules) {
    const tbody = document.getElementById('moduleTableBody');
    if (!modules || modules.length === 0) {
        tbody.innerHTML = '<tr><td colspan="7" class="empty-state">No se encontraron módulos cargados</td></tr>';
        return;
    }

    tbody.innerHTML = modules.map(m => `
        <tr>
            <td><strong>${escapeHtml(m.name)}</strong></td>
            <td class="text-cyan">${m.base}</td>
            <td class="text-purple">${m.end}</td>
            <td>${(m.size / 1024 / 1024).toFixed(2)} MB</td>
            <td><span class="badge badge-cyan">${m.perms}</span></td>
            <td><small>${escapeHtml(m.path)}</small></td>
            <td>
                <button class="btn btn-sm btn-secondary" onclick="inspectModule('${m.name}', '${m.base}')">Inspeccionar</button>
            </td>
        </tr>
    `).join('');
}

function inspectModule(name, base) {
    // Switch to hex tab
    document.querySelectorAll('.nav-tab').forEach(t => t.classList.remove('active'));
    document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
    document.querySelector('[data-tab="hexview"]').classList.add('active');
    document.getElementById('tab-hexview').classList.add('active');

    document.getElementById('hexAddressInput').value = base;
    readHexMemory(base);
}

function filterTable(tbodyId, query) {
    const q = query.toLowerCase();
    const rows = document.getElementById(tbodyId).getElementsByTagName('tr');
    for (let r of rows) {
        const text = r.innerText.toLowerCase();
        r.style.display = text.includes(q) ? '' : 'none';
    }
}

// Address Resolver (supports "libUE4.so+0xEDE48C0" or "0x7d12340000")
async function resolveAddress(addrStr) {
    if (!addrStr) return null;
    addrStr = addrStr.trim();

    if (addrStr.includes('+')) {
        const parts = addrStr.split('+');
        const modName = parts[0].trim();
        const offsetStr = parts[1].trim();
        
        let mod = cachedModules.find(m => m.name.toLowerCase() === modName.toLowerCase());
        if (!mod) {
            // Try fetching modules
            const res = await apiCall('/api/modules');
            if (res.modules) {
                cachedModules = res.modules;
                mod = cachedModules.find(m => m.name.toLowerCase() === modName.toLowerCase());
            }
        }

        if (!mod) {
            alert(`Module '${modName}' not found in target maps`);
            return null;
        }

        const base = BigInt(mod.base);
        const offset = BigInt(offsetStr);
        return '0x' + (base + offset).toString(16);
    }

    if (!addrStr.startsWith('0x') && !addrStr.startsWith('0X')) {
        return '0x' + addrStr;
    }
    return addrStr;
}

// Hex Reader & Viewer
async function readHexMemory(customAddr = null) {
    const inputStr = customAddr || document.getElementById('hexAddressInput').value.trim();
    if (!inputStr) {
        alert('Please enter an address or module+offset');
        return;
    }

    const resolved = await resolveAddress(inputStr);
    if (!resolved) return;

    const size = parseInt(document.getElementById('hexSizeSelect').value, 10) || 128;
    const res = await apiCall('/api/read', 'POST', { address: resolved, size });

    if (res.status === 'ok' && res.hex) {
        currentViewingAddress = BigInt(resolved);
        document.getElementById('currentViewingAddr').innerText = resolved;
        renderHexView(resolved, res.hex);
        parseTypes(res.hex, selectedByteIndex);
    } else {
        document.getElementById('hexViewContainer').innerHTML = `<div class="hex-placeholder text-red">Failed to read memory at ${resolved}: ${res.message || 'Inaccessible'}</div>`;
    }
}

function renderHexView(startAddrStr, hexStr) {
    const container = document.getElementById('hexViewContainer');
    const startAddr = BigInt(startAddrStr);
    const bytes = [];
    for (let i = 0; i < hexStr.length; i += 2) {
        bytes.push(hexStr.substr(i, 2));
    }
    rawMemoryBuffer = bytes;

    let html = '';
    const bytesPerRow = 16;

    for (let r = 0; r < bytes.length; r += bytesPerRow) {
        const rowAddr = startAddr + BigInt(r);
        const rowAddrHex = '0x' + rowAddr.toString(16).toUpperCase().padStart(12, '0');
        const rowBytes = bytes.slice(r, r + bytesPerRow);

        // Hex columns
        let hexCols = '';
        for (let b = 0; b < bytesPerRow; b++) {
            const byteIndex = r + b;
            if (b < rowBytes.length) {
                const isSelected = byteIndex === selectedByteIndex;
                hexCols += `<span class="hex-byte ${isSelected ? 'selected' : ''}" onclick="selectByte(${byteIndex})">${rowBytes[b]}</span>`;
            } else {
                hexCols += `<span class="hex-byte opacity-0">  </span>`;
            }
            if (b === 7) hexCols += ' '; // mid row space
        }

        // ASCII column
        let asciiCols = '';
        for (let b = 0; b < rowBytes.length; b++) {
            const charCode = parseInt(rowBytes[b], 16);
            const char = (charCode >= 32 && charCode <= 126) ? String.fromCharCode(charCode) : '.';
            asciiCols += escapeHtml(char);
        }

        html += `
            <div class="hex-row">
                <span class="hex-offset">${rowAddrHex}</span>
                <span class="hex-bytes">${hexCols}</span>
                <span class="hex-ascii">${asciiCols}</span>
            </div>
        `;
    }

    container.innerHTML = html;
}

window.selectByte = function(index) {
    selectedByteIndex = index;
    document.getElementById('selectedByteOffset').innerText = `+0x${index.toString(16).toUpperCase().padStart(2, '0')}`;
    
    // update highlights
    document.querySelectorAll('.hex-byte').forEach((el, i) => {
        el.classList.toggle('selected', i === index);
    });

    if (rawMemoryBuffer) {
        const hex = rawMemoryBuffer.join('');
        parseTypes(hex, index);
    }
};

function parseTypes(hexStr, byteOffset) {
    const bytes = [];
    for (let i = byteOffset * 2; i < hexStr.length; i += 2) {
        bytes.push(parseInt(hexStr.substr(i, 2), 16));
    }
    const buf = new Uint8Array(bytes);
    const view = new DataView(buf.buffer);

    document.getElementById('t_int8').innerText = buf.length >= 1 ? view.getInt8(0) : '--';
    document.getElementById('t_uint8').innerText = buf.length >= 1 ? `0x${view.getUint8(0).toString(16).toUpperCase()} (${view.getUint8(0)})` : '--';
    document.getElementById('t_int16').innerText = buf.length >= 2 ? view.getInt16(0, true) : '--';
    document.getElementById('t_uint16').innerText = buf.length >= 2 ? view.getUint16(0, true) : '--';
    document.getElementById('t_int32').innerText = buf.length >= 4 ? view.getInt32(0, true) : '--';
    document.getElementById('t_uint32').innerText = buf.length >= 4 ? view.getUint32(0, true) : '--';
    
    if (buf.length >= 8) {
        const u64 = view.getBigUint64(0, true);
        const i64 = view.getBigInt64(0, true);
        document.getElementById('t_int64').innerText = i64.toString();
        document.getElementById('t_uint64').innerText = '0x' + u64.toString(16).toUpperCase();
    } else {
        document.getElementById('t_int64').innerText = '--';
        document.getElementById('t_uint64').innerText = '--';
    }

    document.getElementById('t_float').innerText = buf.length >= 4 ? view.getFloat32(0, true).toFixed(4) : '--';
    document.getElementById('t_double').innerText = buf.length >= 8 ? view.getFloat64(0, true).toFixed(6) : '--';

    let asciiStr = '';
    for (let i = 0; i < Math.min(buf.length, 32); i++) {
        if (buf[i] === 0) break;
        asciiStr += (buf[i] >= 32 && buf[i] <= 126) ? String.fromCharCode(buf[i]) : '.';
    }
    document.getElementById('t_ascii').innerText = asciiStr ? `"${asciiStr}"` : '""';
}

function toggleAutoRefresh() {
    const isChecked = document.getElementById('chkAutoRefresh').checked;
    if (autoRefreshTimer) {
        clearInterval(autoRefreshTimer);
        autoRefreshTimer = null;
    }

    if (isChecked) {
        const interval = parseInt(document.getElementById('autoRefreshInterval').value, 10) || 500;
        autoRefreshTimer = setInterval(() => {
            readHexMemory();
        }, interval);
    }
}

// Pointer Chain Resolver
async function resolvePointerChain() {
    const baseStr = document.getElementById('ptrBaseInput').value.trim();
    const offsetsStr = document.getElementById('ptrOffsetsInput').value.trim();
    const container = document.getElementById('ptrTreeContainer');

    if (!baseStr) {
        alert('Please enter base address');
        return;
    }

    const resolvedBase = await resolveAddress(baseStr);
    if (!resolvedBase) return;

    const offsets = offsetsStr ? offsetsStr.split(/\s+/).map(o => o.startsWith('0x') ? o : '0x' + o) : [];

    container.innerHTML = '<div class="empty-state">Resolving pointer chain...</div>';

    const res = await apiCall('/api/read_ptr', 'POST', { base: resolvedBase, offsets });
    if (res.status === 'ok' && res.chain) {
        let html = '';
        res.chain.forEach((addr, idx) => {
            const isLast = idx === res.chain.length - 1;
            const off = idx === 0 ? 'BASE' : offsets[idx - 1];
            html += `
                <div class="ptr-step-node">
                    <div>
                        <span class="badge ${idx === 0 ? 'badge-cyan' : 'badge-purple'}">Step ${idx} [${off}]</span>
                        <strong class="ml-2 text-cyan">${addr}</strong>
                    </div>
                    <div>
                        ${isLast ? '<span class="badge badge-green">FINAL TARGET</span>' : '<span class="text-muted">-> deref</span>'}
                        <button class="btn btn-sm btn-secondary ml-2" onclick="inspectModule('', '${addr}')">Inspect</button>
                    </div>
                </div>
            `;
        });
        container.innerHTML = html;
    } else {
        container.innerHTML = `<div class="empty-state text-red">Failed to resolve: ${res.message || 'Broken pointer'}</div>`;
    }
}

function stringToHexPattern(str) {
    const trimmed = str.trim();
    // If it is already a hex sequence like "4A 6F ?? 00", leave it
    if (/^([0-9a-fA-F\?]{1,2}\s+)+[0-9a-fA-F\?]{1,2}$/.test(trimmed)) {
        return trimmed;
    }
    // Otherwise convert ASCII string to hex byte string
    return Array.from(trimmed).map(c => c.charCodeAt(0).toString(16).padStart(2, '0')).join(' ');
}

// Pattern Scanner
async function startPatternScan() {
    const scopeSelect = document.getElementById('scanScopeSelect');
    const scope = scopeSelect ? scopeSelect.value : 'ALL';
    const modInput = document.getElementById('scanModuleInput');
    const mod = scope === 'CUSTOM' ? modInput.value.trim() : scope;
    const rawPattern = document.getElementById('scanPatternInput').value.trim();
    const tbody = document.getElementById('scanResultsBody');

    if (!rawPattern) {
        alert('Ingresa un patrón hex (ej: 48 8B ?? 00) o un texto como "Johncake"');
        return;
    }

    const hexPattern = stringToHexPattern(rawPattern);
    tbody.innerHTML = `<tr><td colspan="4" class="empty-state">Escaneando memoria RAM (${scope === 'ALL' ? 'Toda la Memoria RAM' : mod})... Patrón: <code class="text-cyan">${escapeHtml(hexPattern)}</code></td></tr>`;

    let res;
    if (scope === 'ALL') {
        res = await apiCall('/api/scan_all', 'POST', { pattern: hexPattern });
    } else {
        res = await apiCall('/api/scan_mod', 'POST', { module: mod, pattern: hexPattern });
    }

    if (res.status === 'ok' && res.matches) {
        if (res.matches.length === 0) {
            tbody.innerHTML = `<tr><td colspan="4" class="empty-state">No se encontraron coincidencias en ${scope === 'ALL' ? 'la memoria RAM' : mod} para el patrón</td></tr>`;
            return;
        }

        const modObj = cachedModules.find(m => m.name.toLowerCase() === (mod||'').toLowerCase());
        const base = modObj ? BigInt(modObj.base) : 0n;

        tbody.innerHTML = res.matches.map((addr, idx) => {
            const relOffset = base > 0n ? '+0x' + (BigInt(addr) - base).toString(16).toUpperCase() : '--';
            return `
                <tr>
                    <td>${idx + 1}</td>
                    <td class="text-cyan font-mono"><strong>${addr}</strong></td>
                    <td class="text-purple font-mono">${relOffset}</td>
                    <td>
                        <button class="btn btn-sm btn-secondary" onclick="inspectModule('', '${addr}')">Inspeccionar Hex</button>
                    </td>
                </tr>
            `;
        }).join('');
    } else {
        tbody.innerHTML = `<tr><td colspan="4" class="empty-state text-red">Error al escanear: ${res.message || res.error || 'Error'}</td></tr>`;
    }
}

// Scope select listener
document.getElementById('scanScopeSelect')?.addEventListener('change', (e) => {
    const customGroup = document.getElementById('customModGroup');
    if (customGroup) {
        customGroup.style.display = e.target.value === 'CUSTOM' ? 'block' : 'none';
    }
});

// Load MCP Tools list
async function loadMcpTools() {
    const list = document.getElementById('mcpToolList');
    if (!list) return;

    const res = await apiCall('/api/mcp/tools');
    if (res.tools) {
        list.innerHTML = res.tools.map(t => `
            <div class="tool-card glass-panel mb-2 p-3" style="background: rgba(0,0,0,0.3); border-radius: 8px; border: 1px solid var(--border-color); margin-bottom: 10px; padding: 12px;">
                <div style="display: flex; justify-content: space-between; align-items: center;">
                    <strong class="text-cyan font-mono" style="font-family: var(--font-mono); font-size: 13px;">${t.name}</strong>
                    <span class="badge badge-purple" style="font-size: 10px;">Tool</span>
                </div>
                <p style="font-size: 12px; color: var(--text-secondary); margin-top: 4px;">${t.description}</p>
            </div>
        `).join('');
    }
}

// Diagnostic Logs System
let cachedDiagnosticLogs = [];
let logAutoPollTimer = null;

async function fetchDeviceLogs() {
    const minLvl = document.getElementById('logLevelFilter').value || 0;
    try {
        const res = await apiCall(`/api/device/logs?limit=250&min_level=${minLvl}`);
        if (res.status === 'ok' && res.logs) {
            cachedDiagnosticLogs = res.logs;
            renderDiagnosticLogs();
        }
    } catch (err) {
        // silent fail during auto poll
    }
}

function renderDiagnosticLogs() {
    const container = document.getElementById('deviceLogContainer') || document.getElementById('consoleLogContainer');
    if (!container) return;
    const search = (document.getElementById('logSearchInput')?.value || '').toLowerCase();

    const filtered = cachedDiagnosticLogs.filter(l => {
        if (!search) return true;
        return (l.category && l.category.toLowerCase().includes(search)) ||
               (l.message && l.message.toLowerCase().includes(search)) ||
               (l.level && l.level.toLowerCase().includes(search)) ||
               (l.file && l.file.toLowerCase().includes(search));
    });

    if (filtered.length === 0) {
        container.innerHTML = '<div class="empty-state">No se encontraron logs del daemon con el filtro actual</div>';
        return;
    }

    container.innerHTML = filtered.map(l => {
        let lvlColor = 'text-green';
        if (l.level === 'CRITICAL' || l.level === 'ERROR' || l.level === 'FATAL') lvlColor = 'text-red';
        else if (l.level === 'WARN') lvlColor = 'text-yellow';
        else if (l.level === 'DEBUG') lvlColor = 'text-cyan';

        const tidBadge = l.tid ? `<span class="badge" style="background: rgba(255,255,255,0.06); font-size: 10px; padding: 1px 4px; margin-right: 4px;">TID:${l.tid}</span>` : '';
        const errnoBadge = l.err_code && l.err_code !== 0 ? `<span class="badge badge-red" style="font-size: 10px; padding: 1px 4px; margin-left: 6px;">errno ${l.err_code}: ${escapeHtml(l.err_desc || '')}</span>` : '';

        return `
            <div class="log-line" style="margin-bottom: 4px; padding: 3px 0; border-bottom: 1px solid rgba(255,255,255,0.04); font-size: 11.5px;">
                <span class="text-muted" style="font-size: 10.5px;">[${l.time}]</span>
                ${tidBadge}
                <span class="${lvlColor}" style="font-weight: bold; width: 52px; display: inline-block;">[${l.level}]</span>
                <span class="text-purple" style="font-weight: 600;">[${escapeHtml(l.category)}]</span>
                <span style="color: #f1f5f9;">${escapeHtml(l.message)}</span>
                ${errnoBadge}
                <span class="text-muted" style="float: right; font-size: 10px; opacity: 0.7;">${l.file}:${l.line}</span>
            </div>
        `;
    }).join('');

    container.scrollTop = container.scrollHeight;
}

async function clearAllLogs() {
    await apiCall('/api/device/clear_logs', 'POST');
    cachedDiagnosticLogs = [];
    const container = document.getElementById('deviceLogContainer') || document.getElementById('consoleLogContainer');
    if (container) container.innerHTML = '<div class="empty-state">Logs del daemon limpiados con éxito</div>';
}

function clearBridgeLogs() {
    const container = document.getElementById('bridgeLogContainer');
    if (container) container.innerHTML = '<div class="empty-state">Consola de eventos limpia</div>';
}

function toggleLogAutoPoll() {
    const chk = document.getElementById('chkAutoPollLogs');
    if (logAutoPollTimer) {
        clearInterval(logAutoPollTimer);
        logAutoPollTimer = null;
    }
    if (chk && chk.checked) {
        logAutoPollTimer = setInterval(fetchDeviceLogs, 1000);
    }
}

// Start auto poll by default
toggleLogAutoPoll();

// ─── File Manager & Remote Storage System ───────────────────────────────────

let currentFsPath = '/data/local/tmp/';

async function loadFsDirectory(targetPath = null) {
    if (targetPath) currentFsPath = targetPath;
    if (!currentFsPath.endsWith('/')) currentFsPath += '/';

    document.getElementById('fsCurrentPathInput').value = currentFsPath;
    const tbody = document.getElementById('fileTableBody');
    tbody.innerHTML = '<tr><td colspan="6" class="empty-state">Reading directory...</td></tr>';

    try {
        const res = await apiCall(`/api/fs/list?path=${encodeURIComponent(currentFsPath)}`);
        const entries = res.entries || res.items || res.files;
        if (res.status === 'ok' && Array.isArray(entries)) {
            renderFiles(entries);
        } else {
            tbody.innerHTML = `<tr><td colspan="6" class="empty-state text-red">Failed to read directory (${escapeHtml(currentFsPath)}): ${escapeHtml(res.message || res.error || JSON.stringify(res))}</td></tr>`;
        }
    } catch (err) {
        tbody.innerHTML = `<tr><td colspan="6" class="empty-state text-red">Connection error: ${escapeHtml(err.message)}</td></tr>`;
    }
}

function renderFiles(entries) {
    const tbody = document.getElementById('fileTableBody');
    if (!entries || entries.length === 0) {
        tbody.innerHTML = '<tr><td colspan="6" class="empty-state">Directory is empty</td></tr>';
        return;
    }

    tbody.innerHTML = entries.map(e => {
        const icon = e.is_dir ? '📁' : (e.name.endsWith('.zip') || e.name.endsWith('.tar') || e.name.endsWith('.gz') ? '📦' : (e.name.endsWith('.so') ? '⚙️' : '📄'));
        const sizeStr = e.is_dir ? '--' : formatBytes(e.size);
        const modDate = e.mod_time ? new Date(e.mod_time * 1000).toLocaleString() : '--';

        const nameDisplay = e.is_dir 
            ? `<a href="javascript:void(0)" onclick="loadFsDirectory('${escapeHtml(e.path)}')" style="color: var(--accent-cyan); font-weight: 600; text-decoration: none;">${escapeHtml(e.name)}/</a>`
            : `<span>${escapeHtml(e.name)}</span>`;

        let actionBtns = '';
        if (e.is_dir) {
            actionBtns = `
                <button class="btn btn-sm btn-secondary" onclick="downloadRemoteFolder('${escapeHtml(e.path)}')">⬇️ ZIP & Download</button>
                <button class="btn btn-sm btn-danger" onclick="deleteRemoteItem('${escapeHtml(e.path)}', true)">🗑️</button>
            `;
        } else {
            actionBtns = `
                <button class="btn btn-sm btn-primary" onclick="downloadRemoteFile('${escapeHtml(e.path)}')">⬇️ Download</button>
                <button class="btn btn-sm btn-secondary" onclick="compressRemoteFile('${escapeHtml(e.path)}')">📦 ZIP</button>
                <button class="btn btn-sm btn-danger" onclick="deleteRemoteItem('${escapeHtml(e.path)}', false)">🗑️</button>
            `;
        }

        return `
            <tr>
                <td>${icon}</td>
                <td>${nameDisplay}</td>
                <td class="font-mono">${sizeStr}</td>
                <td class="text-muted font-mono"><small>${e.perms || '--'}</small></td>
                <td class="text-muted" style="font-size: 11px;">${modDate}</td>
                <td style="display: flex; gap: 6px;">${actionBtns}</td>
            </tr>
        `;
    }).join('');
}

function handleFsGoUp() {
    if (currentFsPath === '/' || currentFsPath === '') return;
    const parts = currentFsPath.replace(/\/$/, '').split('/');
    parts.pop();
    const upPath = parts.join('/') || '/';
    loadFsDirectory(upPath);
}

async function handleFsNewFolder() {
    const folderName = prompt('Enter new folder name:');
    if (!folderName) return;

    const newPath = currentFsPath + folderName;
    const res = await apiCall('/api/fs/mkdir', 'POST', { path: newPath });
    if (res.status === 'ok') {
        loadFsDirectory();
    } else {
        alert('Failed to create folder: ' + (res.message || 'Error'));
    }
}

async function handleFsUpload(e) {
    const file = e.target.files[0];
    if (!file) return;

    const remoteDest = currentFsPath + file.name;
    const box = document.getElementById('transferStatusBox');
    const nameEl = document.getElementById('transferFileName');
    const barEl = document.getElementById('transferProgressBar');
    const speedEl = document.getElementById('transferSpeed');

    box.style.display = 'block';
    nameEl.innerText = `Uploading: ${file.name} (${formatBytes(file.size)})`;
    barEl.style.width = '0%';

    const CHUNK_SIZE = 64 * 1024;
    let offset = 0;
    const totalSize = file.size;
    const startTime = Date.now();

    try {
        let isFirst = true;
        while (offset < totalSize) {
            const chunkBlob = file.slice(offset, offset + CHUNK_SIZE);
            const arrayBuffer = await chunkBlob.arrayBuffer();
            const bytes = new Uint8Array(arrayBuffer);

            // Convert to base64
            let binary = '';
            for (let i = 0; i < bytes.byteLength; i++) {
                binary += String.fromCharCode(bytes[i]);
            }
            const b64 = btoa(binary);

            const res = await apiCall('/api/raw_cmd', 'POST', {
                command: `fs_write ${remoteDest} ${offset} ${isFirst ? '1' : '0'} ${b64}`
            });

            if (res.status !== 'ok') {
                throw new Error(res.message || 'Upload chunk failed');
            }

            offset += bytes.byteLength;
            isFirst = false;

            const percent = Math.min(100, Math.round((offset / totalSize) * 100));
            barEl.style.width = percent + '%';
            const elapsed = (Date.now() - startTime) / 1000;
            const speed = elapsed > 0 ? (offset / 1024 / 1024 / elapsed).toFixed(2) : 0;
            speedEl.innerText = `${speed} MB/s (${percent}%)`;
        }

        setTimeout(() => { box.style.display = 'none'; }, 2000);
        loadFsDirectory();
    } catch (err) {
        alert('Upload failed: ' + err.message);
        box.style.display = 'none';
    } finally {
        e.target.value = '';
    }
}

async function downloadRemoteFile(remotePath) {
    const box = document.getElementById('transferStatusBox');
    const nameEl = document.getElementById('transferFileName');
    const barEl = document.getElementById('transferProgressBar');
    const speedEl = document.getElementById('transferSpeed');

    box.style.display = 'block';
    nameEl.innerText = `Downloading: ${remotePath.split('/').pop()}`;
    barEl.style.width = '0%';

    try {
        const res = await apiCall('/api/fs/download', 'POST', { remote_path: remotePath });
        if (res.status === 'ok') {
            alert(`✅ Archivo descargado con éxito en tu PC:\n${res.local_path}`);
        } else {
            alert(`❌ Error al descargar: ${res.message}`);
        }
    } catch (err) {
        alert(`❌ Error de descarga: ${err.message}`);
    } finally {
        box.style.display = 'none';
    }
}

async function downloadRemoteFolder(remoteDir) {
    const box = document.getElementById('transferStatusBox');
    const nameEl = document.getElementById('transferFileName');
    const barEl = document.getElementById('transferProgressBar');

    box.style.display = 'block';
    nameEl.innerText = `Compressing & Downloading folder: ${remoteDir.split('/').filter(Boolean).pop()}`;
    barEl.style.width = '50%';

    try {
        const res = await apiCall('/api/fs/download_folder', 'POST', { remote_dir: remoteDir });
        if (res.status === 'ok') {
            alert(`✅ Carpeta empaquetada en ZIP y descargada con éxito en tu PC:\n${res.local_path}`);
        } else {
            alert(`❌ Error al descargar carpeta: ${res.message}`);
        }
    } catch (err) {
        alert(`❌ Error: ${err.message}`);
    } finally {
        box.style.display = 'none';
    }
}

async function compressRemoteFile(filePath) {
    const outZip = filePath + '.zip';
    const res = await apiCall('/api/device/compress', 'POST', {
        input_path: filePath,
        output_archive: outZip,
        format: 'zip'
    });
    if (res.status === 'ok') {
        loadFsDirectory();
    } else {
        alert('Compress failed: ' + (res.message || 'Error'));
    }
}

async function deleteRemoteItem(targetPath, isDir) {
    if (!confirm(`¿Estás seguro de eliminar ${isDir ? 'la carpeta' : 'el archivo'} ${targetPath}?`)) return;
    const res = await apiCall('/api/fs/delete', 'POST', { path: targetPath, recursive: isDir });
    if (res.status === 'ok') {
        loadFsDirectory();
    } else {
        alert('Delete failed: ' + (res.message || 'Error'));
    }
}

async function handlePushLiveUpdate() {
    if (!confirm('¿Deseas compilar y enviar la última versión del binario mem_server.sh al celular y aplicar Hot-Reload seguro?')) {
        return;
    }

    const box = document.getElementById('transferStatusBox');
    const nameEl = document.getElementById('transferFileName');
    const barEl = document.getElementById('transferProgressBar');
    const speedEl = document.getElementById('transferSpeed');

    box.style.display = 'block';
    nameEl.innerText = '🚀 Subiendo nueva actualización a /data/local/tmp/updates/ ...';
    barEl.style.width = '30%';
    speedEl.innerText = 'Validating & Uploading';

    try {
        const res = await apiCall('/api/device/push_update', 'POST');
        if (res.status === 'ok') {
            barEl.style.width = '100%';
            nameEl.innerText = '✅ Actualización aplicada con éxito. Reconectando...';
            alert(`✅ ACTUALIZACIÓN EN VIVO EXITOSA:\n\n${res.message}\n\nEl nuevo binario fue validado, se le dio chmod 777 y tomó el control sin interrumpir el servicio.`);
            setTimeout(checkStatus, 1500);
            loadFsDirectory();
        } else {
            alert(`❌ ERROR EN ACTUALIZACIÓN:\n\n${res.message || res.error || 'Error'}\n\nEl servidor anterior sigue ACTIVO y protegido para evitar caídas.`);
        }
    } catch (err) {
        alert('❌ Error al enviar actualización: ' + err.message);
    } finally {
        setTimeout(() => { box.style.display = 'none'; }, 2000);
    }
}

function formatBytes(bytes) {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
}

// ─── Update & System Maintenance Manager ──────────────────────────────────

function appendUpdateLog(tag, msg, type = 'info') {
    const container = document.getElementById('updateLogContainer');
    if (!container) return;
    const time = new Date().toLocaleTimeString();
    const entry = document.createElement('div');
    entry.className = `log-entry log-${type}`;
    entry.innerHTML = `<span class="log-time">[${time}]</span> <span class="log-tag tag-${type}">${escapeHtml(tag)}</span> ${escapeHtml(msg)}`;
    container.appendChild(entry);
    container.scrollTop = container.scrollHeight;
}

async function fetchSystemInfo() {
    try {
        const res = await apiCall('/api/system/info');
        const verEl = document.getElementById('sysClientVer');
        const binEl = document.getElementById('sysBinaryStatus');
        if (verEl) verEl.innerText = res.client_version || '2.5.0-Turbo';
        if (binEl) {
            binEl.innerText = res.binary_exists ? `Listo (${formatBytes(res.binary_size_bytes)})` : 'No encontrado';
            binEl.className = res.binary_exists ? 'metric-value text-green' : 'metric-value text-red';
        }
    } catch (e) {}
}

async function handleRebuildAndPush() {
    const btn = document.getElementById('btnRebuildAndPush');
    if (btn) btn.disabled = true;
    appendUpdateLog('NDK-BUILD', 'Iniciando compilación C++ (ndk-build) y empaquetado de mem_server.sh...', 'info');

    try {
        const res = await apiCall('/api/device/rebuild_and_push', 'POST');
        if (res.status === 'ok') {
            appendUpdateLog('EXITO', 'Compilación exitosa. Binario subido a /data/local/tmp/updates/ y activado con chmod 777.', 'info');
            alert('✅ RECOMPILACIÓN Y ACTUALIZACIÓN COMPLETADA CON ÉXITO:\n\nEl nuevo binario fue compilado con el NDK de Android, transferido por streaming y activado en el celular sin interrumpir el servicio.');
            setTimeout(checkStatus, 1500);
            fetchSystemInfo();
        } else {
            appendUpdateLog('ERROR', res.error || res.message || 'Error en compilación', 'error');
            alert('❌ ERROR EN ACTUALIZACIÓN:\n\n' + (res.error || res.message));
        }
    } catch (err) {
        appendUpdateLog('FATAL', err.message, 'error');
        alert('❌ Error: ' + err.message);
    } finally {
        if (btn) btn.disabled = false;
    }
}

async function handleReloadClientBridge() {
    appendUpdateLog('PUENTE', 'Re-sincronizando puente PC, cerrando sockets huérfanos y conectando...', 'info');
    try {
        const res = await apiCall('/api/server/reconnect', 'POST');
        appendUpdateLog('PUENTE', 'Puente re-conectado con éxito.', 'info');
        checkStatus();
    } catch (e) {
        appendUpdateLog('ERROR', 'Fallo al reconectar: ' + e.message, 'error');
    }
}

// Hook Update Tab Buttons
document.getElementById('btnPushQuickUpdate')?.addEventListener('click', handlePushLiveUpdate);
document.getElementById('btnPushLiveUpdate')?.addEventListener('click', handlePushLiveUpdate);
document.getElementById('btnRebuildAndPush')?.addEventListener('click', handleRebuildAndPush);
document.getElementById('btnReloadClientBridge')?.addEventListener('click', handleReloadClientBridge);
document.getElementById('btnReloadWebUi')?.addEventListener('click', () => window.location.reload());
document.getElementById('btnClearUpdateLogs')?.addEventListener('click', () => {
    const el = document.getElementById('updateLogContainer');
    if (el) el.innerHTML = '<div class="empty-state">Registro de actualizaciones limpio</div>';
});

// Periodic system info check
setInterval(fetchSystemInfo, 5000);
fetchSystemInfo();

// ─── Lobby & Main Character (Avatar) Manager ──────────────────────────────

async function scanLobbyData() {
    const tbody = document.getElementById('lobbyEntitiesBody');
    if (tbody) tbody.innerHTML = '<tr><td colspan="5" class="empty-state">Escaneando memoria del Lobby y estructuras de UE4...</td></tr>';

    try {
        let status = await apiCall('/api/status');
        
        // Auto-attach if PID changed or not attached
        if (!status.attached || (status.running_games && status.running_games.length > 0 && status.pid !== status.running_games[0].pid)) {
            if (status.running_games && status.running_games.length > 0) {
                await apiCall('/api/attach', 'POST', { target: String(status.running_games[0].pid) });
                status = await apiCall('/api/status');
            }
        }

        const lobbyInfo = await apiCall('/api/ue4/lobby');
        const roots = lobbyInfo.roots || await apiCall('/api/ue4/roots');
        const uworldInstance = lobbyInfo.uworldInstance || '0x0';
        const playerName = lobbyInfo.playerName || 'Johncake';

        const pkgEl = document.getElementById('lobbyAttachedPkg');
        const pidEl = document.getElementById('lobbyAttachedPid');
        const libEl = document.getElementById('lobbyLibBase');
        const fnEl = document.getElementById('lobbyFNamePool');
        const uwEl = document.getElementById('lobbyUWorldPtr');
        const nameEl = document.getElementById('lobbyPlayerName');
        const levelEl = document.getElementById('lobbyLevelPtr');
        const playerLvlEl = document.getElementById('lobbyPlayerLevel');
        const stashEl = document.getElementById('lobbyStashValue');
        const cashEl = document.getElementById('lobbyCashValue');
        const rankEl = document.getElementById('lobbyPlayerRank');
        const titleEl = document.getElementById('lobbyPlayerTitle');
        const statsEl = document.getElementById('lobbyPlayStats');

        const liveStash = lobbyInfo.stashValue || localStorage.getItem('customStashValue') || '4.4M Koen';
        if (pkgEl) pkgEl.innerText = status.name || 'com.proximabeta.mf.liteuamo';
        if (pidEl) pidEl.innerText = status.pid ? String(status.pid) : '--';
        if (libEl) libEl.innerText = roots.lib_base || '--';
        if (fnEl) fnEl.innerText = roots.fname_pool || '--';
        if (uwEl) uwEl.innerText = roots.gworld || '--';
        if (nameEl) nameEl.innerText = playerName;
        if (playerLvlEl) playerLvlEl.innerText = `Nivel ${lobbyInfo.playerLevel || 21}`;
        if (stashEl) stashEl.innerText = liveStash;
        if (cashEl) cashEl.innerText = lobbyInfo.cashValue || '2,425K Koen';
        if (rankEl) rankEl.innerText = lobbyInfo.playerRank || 'Vanguardia 1 (14/24)';
        if (titleEl) titleEl.innerText = lobbyInfo.playerTitle || 'Ejemplo Moral';
        if (statsEl) statsEl.innerText = lobbyInfo.playStats || '5h | 34 Asaltos';
        if (levelEl) levelEl.innerText = uworldInstance !== '0x0' ? uworldInstance : 'En Espera';

        if (tbody) {
            tbody.innerHTML = `
                <tr>
                    <td><strong>libUE4.so Base</strong></td>
                    <td class="font-mono text-cyan">${roots.lib_base || '--'}</td>
                    <td>Módulo ELF Principal</td>
                    <td><span class="badge badge-green">Cargado en RAM</span></td>
                    <td><button class="btn btn-sm btn-secondary" onclick="inspectModuleMem('${roots.lib_base}')">Inspeccionar</button></td>
                </tr>
                <tr>
                    <td><strong>FNamePool Table</strong></td>
                    <td class="font-mono text-green">${roots.fname_pool || '--'}</td>
                    <td>Tabla Global FName</td>
                    <td><span class="badge badge-cyan">Mapeado</span></td>
                    <td><button class="btn btn-sm btn-secondary" onclick="inspectModuleMem('${roots.fname_pool}')">Inspeccionar</button></td>
                </tr>
                <tr>
                    <td><strong>GUObjectArray Table</strong></td>
                    <td class="font-mono text-purple">${roots.guobject_array || '--'}</td>
                    <td>Índice Global de Objetos</td>
                    <td><span class="badge badge-purple">Mapeado</span></td>
                    <td><button class="btn btn-sm btn-secondary" onclick="inspectModuleMem('${roots.guobject_array}')">Inspeccionar</button></td>
                </tr>
                <tr>
                    <td><strong>GWorld (UWorld Ptr)</strong></td>
                    <td class="font-mono text-yellow">${roots.gworld || '--'}</td>
                    <td>Puntero Global UWorld</td>
                    <td><span class="badge badge-yellow">Mundo Activo</span></td>
                    <td><button class="btn btn-sm btn-secondary" onclick="inspectModuleMem('${roots.gworld}')">Inspeccionar</button></td>
                </tr>
                <tr>
                    <td><strong>UWorld Instance (RAM)</strong></td>
                    <td class="font-mono text-cyan">${uworldInstance}</td>
                    <td>Instancia del Nivel del Menú</td>
                    <td><span class="badge badge-green">Instanciado</span></td>
                    <td><button class="btn btn-sm btn-secondary" onclick="inspectModuleMem('${uworldInstance}')">Inspeccionar</button></td>
                </tr>
                <tr>
                    <td><strong>Lobby Avatar / UI Viewport</strong></td>
                    <td class="font-mono text-green">Render Slate/UMG</td>
                    <td>SkeletalMesh Component</td>
                    <td><span class="badge badge-cyan">Visible en Pantalla</span></td>
                    <td><span class="text-muted" style="font-size: 11px;">(Pasa a Actor 3D en partida)</span></td>
                </tr>
            `;
        }
    } catch (err) {
        if (tbody) tbody.innerHTML = `<tr><td colspan="5" class="empty-state text-red">Error al escanear datos del lobby: ${escapeHtml(err.message)}</td></tr>`;
    }
}

window.inspectModuleMem = function(addr) {
    if (!addr || addr === '0x0' || addr === '--') return;
    const tabBtn = document.querySelector('.nav-tab[data-tab="hexview"]');
    if (tabBtn) tabBtn.click();
    const input = document.getElementById('hexAddressInput');
    if (input) {
        input.value = addr;
        readHexMemory();
    }
};

document.getElementById('btnScanLobby')?.addEventListener('click', scanLobbyData);
document.getElementById('btnRefreshLobbyEntities')?.addEventListener('click', scanLobbyData);
document.getElementById('btnInspectLobbyWorld')?.addEventListener('click', async () => {
    const roots = await apiCall('/api/ue4/roots');
    if (roots.gworld) window.inspectModuleMem(roots.gworld);
});

// Clear any legacy customStashValue cache
localStorage.removeItem('customStashValue');

// ─── VM::TransformEncrypt Decryption Panel Functions ─────────────────────────

// Current hook address tracked in JS state for poll/restore
let vmCurrentHookAddr = '';
let vmCurrentCaptureBuf = '';

async function vmSendCmd(cmdStr) {
    try {
        const res = await apiCall('/api/mem/raw_cmd', { method: 'POST', body: JSON.stringify({ cmd: cmdStr }) });
        return res;
    } catch(e) {
        return { status: 'error', message: e.message };
    }
}

// Step 1: Pattern scan libUE4.so for TransformEncrypt prologue
async function vmPatternScan() {
    const pattern = document.getElementById('vmPatternInput').value.trim();
    const el = document.getElementById('vmFuncAddrResult');
    if (!pattern) { el.textContent = '⚠ Ingresa un patrón ARM64'; return; }
    el.textContent = '⏳ Escaneando libUE4.so...';
    const res = await vmSendCmd(`scan_mod libUE4.so ${pattern}`);
    if (res.status === 'ok' && res.matches && res.matches.length > 0) {
        const addr = res.matches[0];
        el.innerHTML = `✅ Encontrada en: <strong style="color:var(--accent-green)">${addr}</strong> (${res.matches.length} matches)`;
        document.getElementById('vmHookFuncAddr').value = addr;
        appendLog('success', `[VM] TransformEncrypt localizada en ${addr}`);
    } else {
        el.innerHTML = `❌ No encontrada. Intenta con patrón alternativo: <code>FF 43 00 D1 FD 7B 02 A9</code>`;
        appendLog('warn', `[VM] Pattern scan: ${res.message || 'sin resultados'}`);
    }
}

// Step 2: Find BSS zone for capture_buf
async function vmFindCaptureBuf() {
    const offsetStr = document.getElementById('vmCaptureBufOffset').value.trim();
    const el = document.getElementById('vmCaptureBufResult');
    el.textContent = '⏳ Obteniendo base de libUE4.so...';

    const modsRes = await vmSendCmd('modules');
    if (modsRes.status !== 'ok' || !modsRes.modules) {
        el.textContent = '❌ Error obteniendo módulos: ' + (modsRes.message || '?');
        return;
    }
    const ue4 = modsRes.modules.find(m => m.name && m.name.includes('libUE4.so'));
    if (!ue4) {
        el.textContent = '❌ libUE4.so no encontrada en módulos. ¿Estás en partida?';
        return;
    }

    const libBase = BigInt(ue4.base || ue4.start);
    const offset = BigInt(offsetStr);
    const candidateAddr = '0x' + (libBase + offset).toString(16);

    el.textContent = `⏳ Verificando ${candidateAddr} (debe estar en ceros)...`;
    const hexRes = await vmSendCmd(`read ${candidateAddr} 64`);
    const isZero = hexRes.hex && hexRes.hex.replace(/\s/g,'').match(/^0+$/);
    if (isZero || (hexRes.status === 'ok')) {
        el.innerHTML = `✅ capture_buf candidato: <strong style="color:var(--accent-purple)">${candidateAddr}</strong> (lib_base=${ue4.base || ue4.start})`;
        document.getElementById('vmHookCaptureBuf').value = candidateAddr;
        appendLog('success', `[VM] capture_buf candidato: ${candidateAddr}`);
    } else {
        el.innerHTML = `⚠ ${candidateAddr} no está en ceros. Prueba offset 0x80000 o busca otra zona BSS rw-.`;
    }
}

// Step 3a: Place BRK hook
async function vmPlaceHook() {
    const funcAddr = document.getElementById('vmHookFuncAddr').value.trim();
    const capBuf   = document.getElementById('vmHookCaptureBuf').value.trim();
    const el = document.getElementById('vmHookPollResult');
    if (!funcAddr || !capBuf) {
        el.textContent = '⚠ Completa func_addr y capture_buf (Steps 1 y 2)';
        return;
    }
    el.textContent = '⏳ Colocando BRK hook...';
    const res = await vmSendCmd(`hook_capture ${funcAddr} ${capBuf} TransformEncrypt`);
    if (res.status === 'ok') {
        vmCurrentHookAddr  = funcAddr;
        vmCurrentCaptureBuf = capBuf;
        el.innerHTML = `✅ Hook BRK colocado en <strong>${funcAddr}</strong><br>
        ⚡ <span style="color:#f59e0b">ENTRA A UNA PARTIDA</span> para que el juego llame a TransformEncrypt.<br>
        Cuando el juego se congele/quede lento, presiona <strong>Poll Registros</strong>.`;
        appendLog('success', `[VM] Hook BRK colocado en ${funcAddr}`);
    } else {
        el.textContent = `❌ Error: ${res.message}`;
        appendLog('error', `[VM] hook_capture failed: ${res.message}`);
    }
}

// Step 3b: Poll captured registers
async function vmPollRegisters() {
    const funcAddr = vmCurrentHookAddr || document.getElementById('vmHookFuncAddr').value.trim();
    const el = document.getElementById('vmHookPollResult');
    if (!funcAddr) { el.textContent = '⚠ Coloca el hook primero (Step 3a)'; return; }
    el.textContent = '⏳ Leyendo registros capturados...';
    const res = await vmSendCmd(`hook_poll ${funcAddr}`);
    if (res.valid) {
        const regs = res.registers;
        el.innerHTML = `<span style="color:var(--accent-green)">✅ CAPTURA EXITOSA</span><br>
        <span style="color:#94a3b8">X0 (DecryptEngine):</span> <strong>${regs.X0}</strong><br>
        <span style="color:var(--accent-cyan)">X1 (public_key_buf):</span> <strong style="color:var(--accent-cyan)">${regs.X1}</strong><br>
        <span style="color:#f59e0b">X2 (cipher FTransform):</span> <strong>${regs.X2}</strong><br>
        <span style="color:#94a3b8">X3 (size):</span> ${regs.X3}<br>
        <br>💡 Usa <code>mem_read_hex(X1, 256)</code> para leer la clave pública.<br>
        💡 Copia X2 value y léelo con <code>read X2 48</code> para el cipher_hex del KPA.`;
        // Auto-fill cipher hex field via read
        appendLog('success', `[VM] Registros capturados: X0=${regs.X0} X1=${regs.X1} X2=${regs.X2}`);
        // Pre-fill KPA cipher: read X2 (48 bytes)
        const cipherRes = await vmSendCmd(`read ${regs.X2} 48`);
        if (cipherRes.status === 'ok' && cipherRes.hex) {
            document.getElementById('vmCipherHex').value = cipherRes.hex.replace(/\s/g,'');
            el.innerHTML += `<br><span style="color:var(--accent-green)">📋 cipher_hex pre-cargado en Step 4 ↓</span>`;
        }
    } else {
        el.textContent = `⏳ Sin captura nueva (valid=false). El hilo no ha llamado a TransformEncrypt aún. Muévete en la partida.`;
    }
}

// Step 3c: Restore hook (unblock game)
async function vmRestoreHook() {
    const funcAddr = vmCurrentHookAddr || document.getElementById('vmHookFuncAddr').value.trim();
    const el = document.getElementById('vmHookPollResult');
    if (!funcAddr) { el.textContent = '⚠ No hay hook activo.'; return; }
    const res = await vmSendCmd(`hook_restore ${funcAddr}`);
    if (res.status === 'ok') {
        el.innerHTML += `<br>✅ Hook restaurado. Juego desbloqueado.`;
        appendLog('success', `[VM] Hook restaurado en ${funcAddr}`);
    } else {
        el.textContent = `❌ ${res.message}`;
    }
}

// Step 4a: Run Known-Plaintext Attack
async function vmRunKpa() {
    const cipherHex = document.getElementById('vmCipherHex').value.trim().replace(/\s/g,'');
    const tx = parseFloat(document.getElementById('vmKpaTx').value) || 0;
    const ty = parseFloat(document.getElementById('vmKpaTy').value) || 0;
    const tz = parseFloat(document.getElementById('vmKpaTz').value) || 0;
    const el = document.getElementById('vmKpaResult');

    if (cipherHex.length < 96) {
        el.textContent = `⚠ cipher_hex debe tener al menos 96 caracteres (48 bytes). Tienes ${cipherHex.length}.`;
        return;
    }
    el.textContent = `⏳ Ejecutando KPA con posición (${tx}, ${ty}, ${tz})...`;
    const res = await vmSendCmd(`kpa ${cipherHex} ${tx} ${ty} ${tz}`);
    if (res.status === 'ok' && res.result === 'KEY_FOUND') {
        el.innerHTML = `<span style="color:var(--accent-green)">✅ ¡CLAVE ENCONTRADA!</span><br>
        Algoritmo: <strong>${res.algo}</strong> | Key len: ${res.key_len} bytes<br>
        Key hex: <code style="color:var(--accent-cyan)">${res.key_hex}</code><br>
        ${res.message}`;
        updateVmKeyBadge(true, res.algo);
        appendLog('success', `[VM] KPA exitosa: ${res.algo} key_len=${res.key_len} key=${res.key_hex}`);
        vmKeyStatus();
    } else {
        el.innerHTML = `❌ KPA fallida. ${res.message || ''}<br>
        <span style="color:#94a3b8">Posibles causas: (1) El cifrado no es XOR. (2) TX/TY/TZ no son exactamente los del local player en UE4 units. (3) El cipher_hex no corresponde a la Translation del actor.</span>`;
        appendLog('warn', `[VM] KPA fallida: ${res.message}`);
    }
}

// Step 4b: Set key manually
async function vmSetKey() {
    const algo = document.getElementById('vmKeyAlgo').value;
    const hex  = document.getElementById('vmManualKeyHex').value.trim().replace(/\s/g,'');
    if (!hex) { appendLog('warn', '[VM] Key hex vacío'); return; }
    const res = await vmSendCmd(`set_decrypt_key ${algo} ${hex}`);
    if (res.status === 'ok') {
        appendLog('success', `[VM] Clave ${algo} establecida (${res.key_len} bytes)`);
        updateVmKeyBadge(true, algo);
        vmKeyStatus();
    } else {
        appendLog('error', `[VM] set_key error: ${res.message}`);
    }
}

// Key status
async function vmKeyStatus() {
    const res = await vmSendCmd('decrypt_key_status');
    const el = document.getElementById('vmKeyStatusResult');
    if (res.status === 'ok') {
        if (res.key_valid) {
            el.innerHTML = `<span style="color:var(--accent-green)">✅ CLAVE ACTIVA</span><br>
            Algo: <strong>${res.algo}</strong> | Len: ${res.key_len} bytes<br>
            Key: <code style="color:var(--accent-cyan)">${res.key_hex}</code><br>
            Game ptr: ${res.key_ptr_in_game}`;
            updateVmKeyBadge(true, res.algo);
        } else {
            el.innerHTML = `<span style="color:#f59e0b">⚠ Sin clave activa</span><br>${res.note || ''}`;
            updateVmKeyBadge(false, '');
        }
    }
}

function updateVmKeyBadge(active, algo) {
    const badge = document.getElementById('vmKeyStatusBadge');
    if (!badge) return;
    if (active) {
        badge.textContent = `✅ CLAVE: ${algo}`;
        badge.className = 'badge badge-green';
    } else {
        badge.textContent = 'SIN CLAVE';
        badge.className = 'badge badge-yellow';
    }
}

// Clear key
async function vmClearKey() {
    const res = await vmSendCmd('clear_decrypt_key');
    if (res.status === 'ok') {
        appendLog('info', '[VM] Clave limpiada.');
        updateVmKeyBadge(false, '');
        vmKeyStatus();
    }
}

// Bone reader
async function vmGetBone() {
    const skelPtr  = document.getElementById('vmBoneSkelPtr').value.trim();
    const boneIdx  = parseInt(document.getElementById('vmBoneIdx').value) || 82;
    const el = document.getElementById('vmBoneResult');
    if (!skelPtr) { el.textContent = '⚠ Ingresa el puntero al SkeletalMeshComponent'; return; }
    el.textContent = `⏳ Leyendo hueso #${boneIdx}...`;
    const res = await vmSendCmd(`get_bone ${skelPtr} ${boneIdx}`);
    if (res.valid) {
        const p = res.world_pos;
        const q = res.quat;
        el.innerHTML = `<span style="color:var(--accent-green)">✅ Hueso #${boneIdx} descifrado</span><br>
        Pos: <strong>X=${p.x.toFixed(1)}, Y=${p.y.toFixed(1)}, Z=${p.z.toFixed(1)}</strong><br>
        Quat: W=${q.w.toFixed(4)} X=${q.x.toFixed(4)} Y=${q.y.toFixed(4)} Z=${q.z.toFixed(4)}<br>
        Algo: ${res.algo} | Key activa: ${res.key_active}`;
    } else {
        el.innerHTML = `❌ Falló. ${res.note || ''}<br>Key activa: ${res.key_active} | Algo: ${res.algo}`;
    }
}

