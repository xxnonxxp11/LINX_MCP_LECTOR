#!/usr/bin/env node
// ============================================================
//  LINX_MCP_LECTOR - Model Context Protocol (MCP) Server for AI
//  Powered by: uam.lol/j
// ============================================================
const readline = require('readline');
const deviceBridge = require('./device_bridge');
const fileTransfer = require('./file_transfer');

// Helper to parse hex dump into types
function parseMemoryBuffer(hexStr) {
    const bytes = [];
    for (let i = 0; i < hexStr.length; i += 2) {
        bytes.push(parseInt(hexStr.substr(i, 2), 16));
    }
    const buf = Buffer.from(bytes);

    const result = {
        hex: hexStr,
        byte_length: buf.length,
        int8: buf.length >= 1 ? buf.readInt8(0) : null,
        uint8: buf.length >= 1 ? buf.readUInt8(0) : null,
        int16_le: buf.length >= 2 ? buf.readInt16LE(0) : null,
        uint16_le: buf.length >= 2 ? buf.readUInt16LE(0) : null,
        int32_le: buf.length >= 4 ? buf.readInt32LE(0) : null,
        uint32_le: buf.length >= 4 ? buf.readUInt32LE(0) : null,
        float_le: buf.length >= 4 ? buf.readFloatLE(0) : null,
        double_le: buf.length >= 8 ? buf.readDoubleLE(0) : null,
        int64_le: buf.length >= 8 ? buf.readBigInt64LE(0).toString() : null,
        uint64_le: buf.length >= 8 ? '0x' + buf.readBigUInt64LE(0).toString(16) : null,
        ascii: buf.toString('ascii').replace(/[^\x20-\x7E]/g, '.')
    };
    return result;
}

const MCP_TOOLS = [
    {
        name: 'mem_connect',
        description: 'Connect or switch connection to Android device by IP address (e.g. 192.168.7.4) or USB (127.0.0.1)',
        inputSchema: {
            type: 'object',
            properties: {
                host: {
                    type: 'string',
                    description: 'IP address of Android device (e.g. "192.168.7.4" or "127.0.0.1")'
                },
                port: {
                    type: 'number',
                    description: 'Daemon TCP port (default: 8088)'
                }
            }
        }
    },
    {
        name: 'mem_status',
        description: 'Get status of Android memory daemon and attached target process',
        inputSchema: {
            type: 'object',
            properties: {}
        }
    },
    {
        name: 'mem_attach',
        description: 'Attach to an Android process by package name or PID (e.g. com.proximabeta.mf.liteuamo)',
        inputSchema: {
            type: 'object',
            properties: {
                target: {
                    type: 'string',
                    description: 'Package name or numeric PID'
                }
            },
            required: ['target']
        }
    },
    {
        name: 'mem_list_processes',
        description: 'List all running processes and PIDs on the Android device',
        inputSchema: {
            type: 'object',
            properties: {}
        }
    },
    {
        name: 'mem_get_modules',
        description: 'Get list of loaded shared libraries (.so modules like libUE4.so) with base addresses, sizes, and permissions',
        inputSchema: {
            type: 'object',
            properties: {}
        }
    },
    {
        name: 'mem_read_hex',
        description: 'Read raw memory bytes from target process as hex string',
        inputSchema: {
            type: 'object',
            properties: {
                address: {
                    type: 'string',
                    description: 'Hex address (e.g. 0x7d12340000)'
                },
                size: {
                    type: 'number',
                    description: 'Number of bytes to read (default: 64, max: 65536)'
                }
            },
            required: ['address']
        }
    },
    {
        name: 'mem_read_types',
        description: 'Read memory at address and interpret as integer, float, pointer, and ascii types',
        inputSchema: {
            type: 'object',
            properties: {
                address: {
                    type: 'string',
                    description: 'Hex address to inspect'
                },
                size: {
                    type: 'number',
                    description: 'Bytes to read for structure parsing (default: 32)'
                }
            },
            required: ['address']
        }
    },
    {
        name: 'mem_read_pointer_chain',
        description: 'Follow and resolve a multi-level pointer chain from a base address through offsets',
        inputSchema: {
            type: 'object',
            properties: {
                base_address: {
                    type: 'string',
                    description: 'Base hex address (e.g. 0x7d12340000)'
                },
                offsets: {
                    type: 'array',
                    items: { type: 'string' },
                    description: 'List of hex offsets (e.g. ["0x10", "0x28", "0x0"])'
                }
            },
            required: ['base_address', 'offsets']
        }
    },
    {
        name: 'mem_read_string',
        description: 'Read a null-terminated UTF-8 / ASCII string from process memory',
        inputSchema: {
            type: 'object',
            properties: {
                address: {
                    type: 'string',
                    description: 'Hex address where string is located'
                },
                max_length: {
                    type: 'number',
                    description: 'Maximum string length (default: 64)'
                }
            },
            required: ['address']
        }
    },
    {
        name: 'mem_write_hex',
        description: 'Write raw bytes to process memory (memory patch)',
        inputSchema: {
            type: 'object',
            properties: {
                address: {
                    type: 'string',
                    description: 'Hex address to write to'
                },
                hex_data: {
                    type: 'string',
                    description: 'Hex string of bytes to write (e.g. 1F2003D5 for NOP)'
                }
            },
            required: ['address', 'hex_data']
        }
    },
    {
        name: 'mem_write_typed',
        description: 'Write typed values (float, int32, int64, vec3, string, double, bool) directly into process memory in real-time',
        inputSchema: {
            type: 'object',
            properties: {
                address: {
                    type: 'string',
                    description: 'Hex address to write to'
                },
                type: {
                    type: 'string',
                    enum: ['float', 'double', 'int32', 'uint32', 'int64', 'int16', 'int8', 'bool', 'vec3', 'string', 'hex'],
                    description: 'Data type to convert and write (default: "float")'
                },
                value: {
                    type: 'string',
                    description: 'Value string (e.g. "90.0", "100", "0x785a0000", "1500 2400 120", "MyString")'
                }
            },
            required: ['address', 'value']
        }
    },
    {
        name: 'mem_patch',
        description: 'Apply a memory patch (hex bytes) at an address and automatically return the original bytes for safe rollback/undo',
        inputSchema: {
            type: 'object',
            properties: {
                address: {
                    type: 'string',
                    description: 'Target hex address'
                },
                hex_patch: {
                    type: 'string',
                    description: 'Hex bytes to write as patch (e.g. "1F2003D5" for ARM64 NOP, "C0035FD6" for RET)'
                }
            },
            required: ['address', 'hex_patch']
        }
    },
    {
        name: 'mem_restore',
        description: 'Rollback and restore original bytes previously saved from mem_patch',
        inputSchema: {
            type: 'object',
            properties: {
                address: {
                    type: 'string',
                    description: 'Target hex address'
                },
                hex_orig: {
                    type: 'string',
                    description: 'Original hex bytes to restore'
                }
            },
            required: ['address', 'hex_orig']
        }
    },
    {
        name: 'mem_pattern_scan',
        description: 'Scan a module or memory range for byte signature pattern (with ?? or ? wildcards)',
        inputSchema: {
            type: 'object',
            properties: {
                module: {
                    type: 'string',
                    description: 'Module name (e.g. libUE4.so)'
                },
                pattern: {
                    type: 'string',
                    description: 'Hex signature pattern with wildcards (e.g. "48 8B ?? 00 20 ? ?")'
                },
                start_address: {
                    type: 'string',
                    description: 'Optional start address if not scanning entire module'
                },
                end_address: {
                    type: 'string',
                    description: 'Optional end address if not scanning entire module'
                }
            },
            required: ['pattern']
        }
    },
    {
        name: 'mem_ue4_roots',
        description: 'Get detected UE4 Root pointers (libUE4.so base, FNamePool, GUObjectArray, GWorld)',
        inputSchema: {
            type: 'object',
            properties: {}
        }
    },
    {
        name: 'mem_resolve_fname',
        description: 'Resolve UE4 FName ComparisonIndex to string name',
        inputSchema: {
            type: 'object',
            properties: {
                index: {
                    type: 'number',
                    description: 'FName comparison index'
                }
            },
            required: ['index']
        }
    },
    {
        name: 'mem_get_uobject',
        description: 'Get UObject pointer, name, and class by global GUObjectArray index',
        inputSchema: {
            type: 'object',
            properties: {
                index: {
                    type: 'number',
                    description: 'Global object index in GUObjectArray'
                }
            },
            required: ['index']
        }
    },
    {
        name: 'mem_get_world_actors',
        description: 'Inspect live game world entities: lists all spawned actors, players, loot, class names, and 3D XYZ coordinates from GWorld->PersistentLevel->AActors',
        inputSchema: {
            type: 'object',
            properties: {
                gworld_address: {
                    type: 'string',
                    description: 'Optional custom GWorld hex address (default: auto-detected)'
                },
                limit: {
                    type: 'number',
                    description: 'Max number of actors to retrieve (default: 512)'
                }
            }
        }
    },
    {
        name: 'mem_inspect_actor',
        description: 'Deep-inspect any live UE4 Actor (e.g. BP_UamCharacter_C): dumps all sub-components, CollisionCylinder location/rotation, SkeletalMesh bones, PlayerState, and internal UObject component fields in real-time',
        inputSchema: {
            type: 'object',
            properties: {
                actor_address: {
                    type: 'string',
                    description: 'Hex pointer address of the Actor (e.g. "0x7430246000")'
                }
            },
            required: ['actor_address']
        }
    },
    {
        name: 'mem_dump_fixed_elf',
        description: 'Dump decrypted shared library (e.g. libUE4.so) directly from game RAM and rebuild Section Headers (.dynsym, .dynstr, .rel.dyn, .rel.plt) and relative relocations so it can be decompiled in IDA Pro or Ghidra',
        inputSchema: {
            type: 'object',
            properties: {
                module: {
                    type: 'string',
                    description: 'Module name to dump (default: "libUE4.so")'
                },
                output_path: {
                    type: 'string',
                    description: 'Output path on Android device (default: "/data/local/tmp/dumped_fixed.so")'
                }
            }
        }
    },
    {
        name: 'mem_get_ue4_config',
        description: 'Get currently active dynamic offsets and feature configuration in memory daemon (PersistentLevel, Actors array, RootComponent, Mesh, CameraManager, Bones, ComponentToWorld)',
        inputSchema: {
            type: 'object',
            properties: {}
        }
    },
    {
        name: 'mem_set_ue4_config',
        description: 'Hot-tune and override UE4 offsets or settings dynamically in real-time without recompiling or restarting. Key can be: "persistent_level", "actors", "root_comp", "mesh", "camera", "bone_array", "comp_to_world", "bone_decrypt", or "reset"',
        inputSchema: {
            type: 'object',
            properties: {
                key: {
                    type: 'string',
                    description: 'Config key name (e.g. "camera", "actors", "root_comp", "mesh", "persistent_level", "bone_array", "comp_to_world", "reset")'
                },
                value: {
                    type: 'string',
                    description: 'New hex or decimal value (e.g. "0x1100", "0x98", "0x158", "0x370")'
                }
            },
            required: ['key', 'value']
        }
    },
    {
        name: 'mem_get_draw_config',
        description: 'Get active on-screen ESP / overlay draw flags (2D/3D boxes, skeleton bones, snaplines, distance, health, weapon, radar, FOV circle, bot filter, supplies loot price)',
        inputSchema: {
            type: 'object',
            properties: {}
        }
    },
    {
        name: 'mem_set_draw_config',
        description: 'Toggle on-screen ESP features in real time. Key: "box", "skeleton", "snapline", "name", "distance", "health", "weapon", "radar", "fov_circle", "ignore_bots", "loot", "loot_price", "min_loot_price", "fov_radius", or "reset". Value: "1"/"0" or number',
        inputSchema: {
            type: 'object',
            properties: {
                key: {
                    type: 'string',
                    description: 'ESP feature key (e.g. "box", "skeleton", "snapline", "distance", "health", "weapon", "radar", "fov_circle", "ignore_bots", "loot", "min_loot_price")'
                },
                value: {
                    type: 'string',
                    description: 'Value: "1" (enable), "0" (disable), or number value (e.g. "5000" for min price)'
                }
            },
            required: ['key', 'value']
        }
    },
    {
        name: 'mem_scan_ue4_roots',
        description: 'Dynamically scan and locate FNamePool, GUObjectArray, and GWorld using ARM64 ADRP+ADD/LDR assembly pattern signatures without hardcoded offsets',
        inputSchema: {
            type: 'object',
            properties: {}
        }
    },
    {
        name: 'mem_get_device_logs',
        description: 'Fetch ultra-detailed internal diagnostic and error logs from the Android memory daemon (includes file/line origins, errno, SELinux, and memory access status)',
        inputSchema: {
            type: 'object',
            properties: {
                limit: {
                    type: 'number',
                    description: 'Number of log entries to retrieve (default: 100)'
                },
                min_level: {
                    type: 'number',
                    description: 'Minimum log level: 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR, 4=CRITICAL (default: 0)'
                }
            }
        }
    },
    {
        name: 'mem_clear_device_logs',
        description: 'Clear the persistent and in-memory diagnostic logs on the Android device',
        inputSchema: {
            type: 'object',
            properties: {}
        }
    },
    {
        name: 'mem_compress_archive',
        description: 'Compress files or directories on Android into zip, tar, tar.gz (tgz), or tar.xz archives directly through the memory daemon',
        inputSchema: {
            type: 'object',
            properties: {
                input_path: {
                    type: 'string',
                    description: 'File or directory path on Android to compress (e.g. "/data/local/tmp/dumped_fixed.so")'
                },
                output_archive: {
                    type: 'string',
                    description: 'Output archive path (e.g. "/data/local/tmp/dumped.zip" or "/data/local/tmp/sdk.tar.gz")'
                },
                format: {
                    type: 'string',
                    enum: ['zip', 'tar', 'tar.gz', 'tar.xz'],
                    description: 'Archive format: "zip", "tar", "tar.gz", or "tar.xz" (default: "zip")'
                }
            },
            required: ['input_path', 'output_archive']
        }
    },
    {
        name: 'mem_decompress_archive',
        description: 'Extract zip, tar, tar.gz, or tar.xz archives on Android to a target directory',
        inputSchema: {
            type: 'object',
            properties: {
                archive_path: {
                    type: 'string',
                    description: 'Archive file path to extract (e.g. "/data/local/tmp/dumped.zip")'
                },
                output_dir: {
                    type: 'string',
                    description: 'Target directory to extract files to (default: "/data/local/tmp/")'
                }
            },
            required: ['archive_path']
        }
    },
    {
        name: 'mem_fs_list',
        description: 'List files and directories on Android device with sizes, permissions, and timestamps',
        inputSchema: {
            type: 'object',
            properties: {
                path: {
                    type: 'string',
                    description: 'Directory path to list (default: "/data/local/tmp/")'
                }
            }
        }
    },
    {
        name: 'mem_file_download',
        description: 'Download any file from Android to local PC with automatic chunking and CRC verification',
        inputSchema: {
            type: 'object',
            properties: {
                remote_path: {
                    type: 'string',
                    description: 'Remote file path on Android (e.g. "/data/local/tmp/libUE4_dumped_fixed.so")'
                },
                local_path: {
                    type: 'string',
                    description: 'Local path on PC to save the downloaded file'
                }
            },
            required: ['remote_path', 'local_path']
        }
    },
    {
        name: 'mem_folder_download',
        description: 'Download an entire folder from Android by compressing it to zip on device and downloading to PC',
        inputSchema: {
            type: 'object',
            properties: {
                remote_dir: {
                    type: 'string',
                    description: 'Remote folder path on Android (e.g. "/data/local/tmp/SDK/")'
                },
                local_zip_path: {
                    type: 'string',
                    description: 'Local path on PC to save the downloaded ZIP file'
                }
            },
            required: ['remote_dir', 'local_zip_path']
        }
    },
    {
        name: 'mem_fs_delete',
        description: 'Delete a file or directory on Android device',
        inputSchema: {
            type: 'object',
            properties: {
                path: {
                    type: 'string',
                    description: 'File or directory path to delete'
                },
                recursive: {
                    type: 'boolean',
                    description: 'If true, recursively deletes directories'
                }
            },
            required: ['path']
        }
    },
    {
        name: 'mem_push_auto_update',
        description: 'Upload latest compiled mem_server.sh to Android, test execution with pre-validation, and perform hot-handoff update without downtime',
        inputSchema: {
            type: 'object',
            properties: {}
        }
    },
    {
        name: 'mem_get_manual',
        description: 'Returns the complete AI Reference Manual and workflow guide detailing all tools, memory inspection, UE4 reflection, and file transfer capabilities',
        inputSchema: {
            type: 'object',
            properties: {}
        }
    },

    // ─── ARM64 Hook Capture Tools (Reverse Engineering / Deobfuscation) ────────────────
    {
        name: 'mem_hook_capture',
        description: 'Place a stealth ARM64 BRK trampoline at a function address to capture X0-X7 argument registers on the next call (for reverse engineering and deobfuscation). Works without ptrace or .so injection. Use mem_hook_poll to retrieve the captured registers. Example: hook VM::TransformEncrypt to extract the public key buffer pointer from X1.',
        inputSchema: {
            type: 'object',
            properties: {
                func_addr: {
                    type: 'string',
                    description: 'Hex address of the target function (e.g. "0x7d1a3bc000" — must be in an executable module like libUE4.so). Get this from mem_pattern_scan first.'
                },
                capture_buf: {
                    type: 'string',
                    description: 'Hex address of a 96-byte writable region in the process (e.g. BSS or heap area). Can be any zeroed writable address. Used as dump buffer for X0-X7.'
                },
                label: {
                    type: 'string',
                    description: 'Human-readable label for this hook (e.g. "TransformEncrypt", "DecryptEngine"). Used in hook_list and hook_poll results.'
                }
            },
            required: ['func_addr', 'capture_buf']
        }
    },
    {
        name: 'mem_hook_restore',
        description: 'Remove a previously placed hook trampoline and restore the original function bytes. Call this after capturing registers so the game continues normally.',
        inputSchema: {
            type: 'object',
            properties: {
                func_addr: {
                    type: 'string',
                    description: 'Hex address of the hooked function to restore'
                }
            },
            required: ['func_addr']
        }
    },
    {
        name: 'mem_hook_poll',
        description: 'Check if a hooked function was called and return the captured X0-X7 argument registers. Returns valid:true only if the function was hit since the last poll. On BRK hooks, also reads the register frame from the stopped thread and auto-restores the hook.',
        inputSchema: {
            type: 'object',
            properties: {
                func_addr: {
                    type: 'string',
                    description: 'Hex address of the hooked function to poll for register data'
                }
            },
            required: ['func_addr']
        }
    },
    {
        name: 'mem_hook_list',
        description: 'List all currently active hook trampolines with their addresses, labels, and capture buffer locations.',
        inputSchema: {
            type: 'object',
            properties: {}
        }
    }
];

const http = require('http');

function httpApiCall(apiPath, method = 'GET', body = null, timeoutMs = 4000) {
    return new Promise((resolve) => {
        const req = http.request({
            hostname: '127.0.0.1',
            port: 3000,
            path: apiPath,
            method: method,
            headers: { 'Content-Type': 'application/json' },
            timeout: timeoutMs
        }, (res) => {
            let data = '';
            res.on('data', (chunk) => data += chunk);
            res.on('end', () => {
                try {
                    resolve({ ok: true, data: JSON.parse(data) });
                } catch (e) {
                    resolve({ ok: true, data: data });
                }
            });
        });
        req.on('error', () => resolve({ ok: false }));
        req.on('timeout', () => { req.destroy(); resolve({ ok: false }); });
        if (body) {
            req.write(JSON.stringify(body));
        }
        req.end();
    });
}

class McpServer {
    constructor() {
        this.device = deviceBridge;
        this.fileTransfer = fileTransfer;
    }

    async isLocalApiAlive() {
        const check = await httpApiCall('/api/config/last_host', 'GET', null, 1000);
        return check.ok && check.data && typeof check.data.port !== 'undefined';
    }

    async autoConnect() {
        if (!this.device.isConnected) {
            try {
                await this.device.smartConnect('127.0.0.1', 8088);
            } catch (e) {
                // Ignore silent fail on auto-connect attempt
            }
        }
    }

    async handleCallTool(name, args) {
        // Fast-path: Check if local web server (index.js) is already handling the connection
        const useHttp = await this.isLocalApiAlive();

        if (useHttp) {
            switch (name) {
                case 'mem_connect': {
                    const host = args.host || '127.0.0.1';
                    const port = args.port || 8088;
                    const res = await httpApiCall('/api/connect', 'POST', { host, port });
                    return { content: [{ type: 'text', text: JSON.stringify(res.data, null, 2) }] };
                }
                case 'mem_status': {
                    const res = await httpApiCall('/api/status', 'GET');
                    return { content: [{ type: 'text', text: JSON.stringify(res.data, null, 2) }] };
                }
                case 'mem_attach': {
                    const res = await httpApiCall('/api/attach', 'POST', { target: args.target });
                    return { content: [{ type: 'text', text: JSON.stringify(res.data, null, 2) }] };
                }
                case 'mem_list_processes': {
                    const res = await httpApiCall('/api/processes', 'GET');
                    return { content: [{ type: 'text', text: JSON.stringify(res.data, null, 2) }] };
                }
                case 'mem_get_modules': {
                    const res = await httpApiCall('/api/modules', 'GET');
                    return { content: [{ type: 'text', text: JSON.stringify(res.data, null, 2) }] };
                }
                case 'mem_read_hex': {
                    const size = args.size || 64;
                    const res = await httpApiCall('/api/read', 'POST', { address: args.address, size });
                    return { content: [{ type: 'text', text: JSON.stringify(res.data, null, 2) }] };
                }
                case 'mem_read_types': {
                    const size = args.size || 32;
                    const res = await httpApiCall('/api/read', 'POST', { address: args.address, size });
                    if (res.ok && res.data && res.data.status === 'ok' && res.data.hex) {
                        const parsed = parseMemoryBuffer(res.data.hex);
                        return { content: [{ type: 'text', text: JSON.stringify({ address: args.address, ...parsed }, null, 2) }] };
                    }
                    return { content: [{ type: 'text', text: JSON.stringify(res.data, null, 2) }] };
                }
                case 'mem_read_pointer_chain': {
                    const res = await httpApiCall('/api/read_ptr', 'POST', { base: args.base_address, offsets: args.offsets || [] });
                    return { content: [{ type: 'text', text: JSON.stringify(res.data, null, 2) }] };
                }
                case 'mem_read_string': {
                    const max_len = args.max_length || 64;
                    const res = await httpApiCall('/api/read_str', 'POST', { address: args.address, max_len });
                    return { content: [{ type: 'text', text: JSON.stringify(res.data, null, 2) }] };
                }
                case 'mem_write_hex': {
                    const res = await httpApiCall('/api/write', 'POST', { address: args.address, hex_data: args.hex_data });
                    return { content: [{ type: 'text', text: JSON.stringify(res.data, null, 2) }] };
                }
                case 'mem_write_typed': {
                    const res = await httpApiCall('/api/write_typed', 'POST', { address: args.address, type: args.type || 'float', value: args.value });
                    return { content: [{ type: 'text', text: JSON.stringify(res.data, null, 2) }] };
                }
                case 'mem_patch': {
                    const res = await httpApiCall('/api/patch', 'POST', { address: args.address, hex_patch: args.hex_patch });
                    return { content: [{ type: 'text', text: JSON.stringify(res.data, null, 2) }] };
                }
                case 'mem_restore': {
                    const res = await httpApiCall('/api/restore', 'POST', { address: args.address, hex_orig: args.hex_orig });
                    return { content: [{ type: 'text', text: JSON.stringify(res.data, null, 2) }] };
                }
                case 'mem_pattern_scan': {
                    if (args.module) {
                        const res = await httpApiCall('/api/scan_mod', 'POST', { module: args.module, pattern: args.pattern });
                        return { content: [{ type: 'text', text: JSON.stringify(res.data, null, 2) }] };
                    } else if (args.start_address && args.end_address) {
                        const res = await httpApiCall('/api/scan', 'POST', { start: args.start_address, end: args.end_address, pattern: args.pattern });
                        return { content: [{ type: 'text', text: JSON.stringify(res.data, null, 2) }] };
                    }
                    return { isError: true, content: [{ type: 'text', text: 'Error: Must provide either module or start_address & end_address' }] };
                }
                case 'mem_ue4_roots':
                case 'mem_scan_ue4_roots': {
                    const res = await httpApiCall('/api/ue4/roots', 'GET');
                    return { content: [{ type: 'text', text: JSON.stringify(res.data, null, 2) }] };
                }
                case 'mem_resolve_fname': {
                    const res = await httpApiCall(`/api/ue4/fname/${args.index}`, 'GET');
                    return { content: [{ type: 'text', text: JSON.stringify(res.data, null, 2) }] };
                }
                case 'mem_get_uobject': {
                    const res = await httpApiCall(`/api/ue4/uobj/${args.index}`, 'GET');
                    return { content: [{ type: 'text', text: JSON.stringify(res.data, null, 2) }] };
                }
                case 'mem_get_world_actors': {
                    const gworld = args.gworld_address || 0;
                    const limit = args.limit || 512;
                    const res = await httpApiCall(`/api/ue4/actors?gworld=${encodeURIComponent(gworld)}&limit=${limit}`, 'GET');
                    return { content: [{ type: 'text', text: JSON.stringify(res.data, null, 2) }] };
                }
                case 'mem_inspect_actor': {
                    const res = await httpApiCall(`/api/ue4/actor/inspect?actor=${encodeURIComponent(args.actor_address)}`, 'GET');
                    return { content: [{ type: 'text', text: JSON.stringify(res.data, null, 2) }] };
                }
                case 'mem_get_ue4_config': {
                    const res = await httpApiCall('/api/ue4/config', 'GET');
                    return { content: [{ type: 'text', text: JSON.stringify(res.data, null, 2) }] };
                }
                case 'mem_set_ue4_config': {
                    const res = await httpApiCall('/api/ue4/config', 'POST', { key: args.key, value: args.value });
                    return { content: [{ type: 'text', text: JSON.stringify(res.data, null, 2) }] };
                }
                case 'mem_get_draw_config': {
                    const res = await httpApiCall('/api/draw/config', 'GET');
                    return { content: [{ type: 'text', text: JSON.stringify(res.data, null, 2) }] };
                }
                case 'mem_set_draw_config': {
                    const res = await httpApiCall('/api/draw/config', 'POST', { key: args.key, value: args.value });
                    return { content: [{ type: 'text', text: JSON.stringify(res.data, null, 2) }] };
                }
                case 'mem_dump_fixed_elf': {
                    const modName = args.module || 'libUE4.so';
                    const outPath = args.output_path || '/data/local/tmp/dumped_fixed.so';
                    const res = await httpApiCall('/api/ue4/dump_elf', 'POST', { module: modName, output_path: outPath });
                    return { content: [{ type: 'text', text: JSON.stringify(res.data, null, 2) }] };
                }
                case 'mem_get_device_logs': {
                    const limit = args.limit || 100;
                    const minLvl = args.min_level || 0;
                    const res = await httpApiCall(`/api/device/logs?limit=${limit}&min_level=${minLvl}`, 'GET');
                    return { content: [{ type: 'text', text: JSON.stringify(res.data, null, 2) }] };
                }
                case 'mem_clear_device_logs': {
                    const res = await httpApiCall('/api/device/clear_logs', 'POST', {});
                    return { content: [{ type: 'text', text: JSON.stringify(res.data, null, 2) }] };
                }
                case 'mem_fs_list': {
                    const p = args.path || '/data/local/tmp/';
                    const res = await httpApiCall(`/api/fs/list?path=${encodeURIComponent(p)}`, 'GET');
                    return { content: [{ type: 'text', text: JSON.stringify(res.data, null, 2) }] };
                }
                case 'mem_compress_archive': {
                    const res = await httpApiCall('/api/device/compress', 'POST', { input_path: args.input_path, output_archive: args.output_archive, format: args.format || 'zip' });
                    return { content: [{ type: 'text', text: JSON.stringify(res.data, null, 2) }] };
                }
                case 'mem_decompress_archive': {
                    const res = await httpApiCall('/api/device/decompress', 'POST', { archive_path: args.archive_path, output_dir: args.output_dir || '/data/local/tmp/' });
                    return { content: [{ type: 'text', text: JSON.stringify(res.data, null, 2) }] };
                }
                case 'mem_get_manual': {
                    const fs = require('fs');
                    const path = require('path');
                    const manualPath = path.join(__dirname, '../../AI_MCP_MANUAL.md');
                    let manualContent = fs.existsSync(manualPath) ? fs.readFileSync(manualPath, 'utf8') : '# Android Memory MCP Tools Manual';
                    return { content: [{ type: 'text', text: manualContent }] };
                }
            }
        }

        // Direct Socket Fallback
        await this.autoConnect();

        switch (name) {
            case 'mem_connect': {
                const targetHost = args.host || this.device.getLastSavedHost() || '127.0.0.1';
                const targetPort = args.port || 8088;
                try {
                    const res = await this.device.connect(targetHost, targetPort);
                    return { content: [{ type: 'text', text: JSON.stringify({ success: true, message: `Connected to ${targetHost}:${targetPort}`, ...res }, null, 2) }] };
                } catch (e) {
                    return { content: [{ type: 'text', text: JSON.stringify({ success: false, error: e.message }, null, 2) }] };
                }
            }

            case 'mem_status': {
                if (!this.device.isConnected) {
                    return { content: [{ type: 'text', text: JSON.stringify({ connected: false, message: 'Not connected to Android daemon. Make sure daemon is running on port 8088.' }) }] };
                }
                await this.device.autoAttachGame();
                const res = await this.device.getStatus();
                const running = await this.device.detectRunningGames();
                return { content: [{ type: 'text', text: JSON.stringify({ ...res, running_games: running }, null, 2) }] };
            }

            case 'mem_attach': {
                const res = await this.device.attach(args.target);
                return { content: [{ type: 'text', text: JSON.stringify(res, null, 2) }] };
            }

            case 'mem_list_processes': {
                const res = await this.device.listProcesses();
                const runningGames = await this.device.detectRunningGames();
                return { content: [{ type: 'text', text: JSON.stringify({ ...res, running_games: runningGames }, null, 2) }] };
            }

            case 'mem_get_modules': {
                const res = await this.device.getModules();
                return { content: [{ type: 'text', text: JSON.stringify(res, null, 2) }] };
            }

            case 'mem_read_hex': {
                const size = args.size || 64;
                const res = await this.device.readMemory(args.address, size);
                return { content: [{ type: 'text', text: JSON.stringify(res, null, 2) }] };
            }

            case 'mem_read_types': {
                const size = args.size || 32;
                const res = await this.device.readMemory(args.address, size);
                if (res.status === 'ok' && res.hex) {
                    const parsed = parseMemoryBuffer(res.hex);
                    return { content: [{ type: 'text', text: JSON.stringify({ address: args.address, ...parsed }, null, 2) }] };
                }
                return { content: [{ type: 'text', text: JSON.stringify(res, null, 2) }] };
            }

            case 'mem_read_pointer_chain': {
                const res = await this.device.readPointerChain(args.base_address, args.offsets || []);
                return { content: [{ type: 'text', text: JSON.stringify(res, null, 2) }] };
            }

            case 'mem_read_string': {
                const maxLen = args.max_length || 64;
                const res = await this.device.readString(args.address, maxLen);
                return { content: [{ type: 'text', text: JSON.stringify(res, null, 2) }] };
            }

            case 'mem_write_hex': {
                const res = await this.device.writeMemory(args.address, args.hex_data);
                return { content: [{ type: 'text', text: JSON.stringify(res, null, 2) }] };
            }

            case 'mem_pattern_scan': {
                if (args.module) {
                    const res = await this.device.patternScanModule(args.module, args.pattern);
                    return { content: [{ type: 'text', text: JSON.stringify(res, null, 2) }] };
                } else if (args.start_address && args.end_address) {
                    const res = await this.device.patternScan(args.start_address, args.end_address, args.pattern);
                    return { content: [{ type: 'text', text: JSON.stringify(res, null, 2) }] };
                } else {
                    return {
                        isError: true,
                        content: [{ type: 'text', text: 'Error: Must provide either module name or start_address & end_address' }]
                    };
                }
            }

            case 'mem_ue4_roots': {
                const res = await this.device.getUE4Roots();
                return { content: [{ type: 'text', text: JSON.stringify(res, null, 2) }] };
            }

            case 'mem_resolve_fname': {
                const res = await this.device.resolveFName(args.index);
                return { content: [{ type: 'text', text: JSON.stringify(res, null, 2) }] };
            }

            case 'mem_get_uobject': {
                const res = await this.device.getUObject(args.index);
                return { content: [{ type: 'text', text: JSON.stringify(res, null, 2) }] };
            }

            case 'mem_get_world_actors': {
                const gworld = args.gworld_address || 0;
                const limit = args.limit || 512;
                const res = await this.device.getWorldActors(gworld, limit);
                return { content: [{ type: 'text', text: JSON.stringify(res, null, 2) }] };
            }

            case 'mem_dump_fixed_elf': {
                const modName = args.module || 'libUE4.so';
                const outPath = args.output_path || '/data/local/tmp/dumped_fixed.so';
                const res = await this.device.dumpELF(modName, outPath);
                return { content: [{ type: 'text', text: JSON.stringify(res, null, 2) }] };
            }

            case 'mem_scan_ue4_roots': {
                const res = await this.device.getUE4Roots();
                return { content: [{ type: 'text', text: JSON.stringify(res, null, 2) }] };
            }

            case 'mem_get_device_logs': {
                const limit = args.limit || 100;
                const minLvl = args.min_level || 0;
                const res = await this.device.getLogs(limit, minLvl);
                return { content: [{ type: 'text', text: JSON.stringify(res, null, 2) }] };
            }

            case 'mem_clear_device_logs': {
                const res = await this.device.clearLogs();
                return { content: [{ type: 'text', text: JSON.stringify(res, null, 2) }] };
            }

            case 'mem_compress_archive': {
                const inPath = args.input_path;
                const outPath = args.output_archive;
                const fmt = args.format || 'zip';
                const res = await this.device.compress(inPath, outPath, fmt);
                return { content: [{ type: 'text', text: JSON.stringify(res, null, 2) }] };
            }

            case 'mem_decompress_archive': {
                const archivePath = args.archive_path;
                const outDir = args.output_dir || '/data/local/tmp/';
                const res = await this.device.decompress(archivePath, outDir);
                return { content: [{ type: 'text', text: JSON.stringify(res, null, 2) }] };
            }

            case 'mem_fs_list': {
                const p = args.path || '/data/local/tmp/';
                const res = await this.fileTransfer.listDirectory(p);
                return { content: [{ type: 'text', text: JSON.stringify(res, null, 2) }] };
            }

            case 'mem_file_download': {
                const res = await this.fileTransfer.downloadFile(args.remote_path, args.local_path);
                return { content: [{ type: 'text', text: JSON.stringify(res, null, 2) }] };
            }

            case 'mem_folder_download': {
                const res = await this.fileTransfer.downloadDirectory(args.remote_dir, args.local_zip_path);
                return { content: [{ type: 'text', text: JSON.stringify(res, null, 2) }] };
            }

            case 'mem_fs_delete': {
                const res = await this.fileTransfer.deleteItem(args.path, args.recursive || false);
                return { content: [{ type: 'text', text: JSON.stringify(res, null, 2) }] };
            }

            case 'mem_push_auto_update': {
                const path = require('path');
                const localBinary = path.join(__dirname, '../../android_server/libs/arm64-v8a/mem_server.sh');
                const updateDir = '/data/local/tmp/updates/';
                const updateRemoteFile = '/data/local/tmp/updates/mem_server_new.sh';
                const liveTargetFile = '/data/local/tmp/mem_server.sh';

                await this.fileTransfer.makeDir(updateDir);
                const uploadRes = await this.fileTransfer.uploadFile(localBinary, updateRemoteFile, true);
                const updateRes = await this.device.updateServer(updateRemoteFile, liveTargetFile);
                return { content: [{ type: 'text', text: JSON.stringify({ upload: uploadRes, update: updateRes }, null, 2) }] };
            }

            case 'mem_get_manual': {
                const fs = require('fs');
                const path = require('path');
                const manualPath = path.join(__dirname, '../../AI_MCP_MANUAL.md');
                let manualContent = '';
                if (fs.existsSync(manualPath)) {
                    manualContent = fs.readFileSync(manualPath, 'utf8');
                } else {
                    manualContent = '# Android Memory MCP Tools Manual\nSee tool definitions for usage.';
                }
                return {
                    content: [
                        {
                            type: 'text',
                            text: manualContent
                        }
                    ]
                };
            }

            case 'mem_hook_capture': {
                const res = await this.device.sendCommand(`hook_capture ${args.func_addr} ${args.capture_buf} ${args.label || 'hook'}`);
                return { content: [{ type: 'text', text: JSON.stringify(res, null, 2) }] };
            }

            case 'mem_hook_restore': {
                const res = await this.device.sendCommand(`hook_restore ${args.func_addr}`);
                return { content: [{ type: 'text', text: JSON.stringify(res, null, 2) }] };
            }

            case 'mem_hook_poll': {
                const res = await this.device.sendCommand(`hook_poll ${args.func_addr}`);
                return { content: [{ type: 'text', text: JSON.stringify(res, null, 2) }] };
            }

            case 'mem_hook_list': {
                const res = await this.device.sendCommand('hook_list');
                return { content: [{ type: 'text', text: JSON.stringify(res, null, 2) }] };
            }

            default:
                return {
                    isError: true,
                    content: [{ type: 'text', text: `Unknown tool: ${name}` }]
                };
        }
    }

    startStdio() {
        const rl = readline.createInterface({
            input: process.stdin,
            output: process.stdout,
            terminal: false
        });

        rl.on('line', async (line) => {
            if (!line.trim()) return;
            try {
                const req = JSON.parse(line);
                const res = await this.handleJsonRpc(req);
                if (res) {
                    process.stdout.write(JSON.stringify(res) + '\n');
                }
            } catch (err) {
                const errRes = {
                    jsonrpc: '2.0',
                    id: null,
                    error: { code: -32700, message: 'Parse error', data: err.message }
                };
                process.stdout.write(JSON.stringify(errRes) + '\n');
            }
        });
    }

    async handleJsonRpc(req) {
        const { id, method, params } = req;
        const fs = require('fs');
        const path = require('path');

        switch (method) {
            case 'initialize':
                return {
                    jsonrpc: '2.0',
                    id,
                    result: {
                        protocolVersion: '2024-11-05',
                        capabilities: {
                            tools: {},
                            resources: {},
                            prompts: {}
                        },
                        serverInfo: {
                            name: 'android-root-memory-mcp',
                            version: '1.0.0'
                        }
                    }
                };

            case 'notifications/initialized':
                return null;

            case 'resources/list':
                return {
                    jsonrpc: '2.0',
                    id,
                    result: {
                        resources: [
                            {
                                uri: 'android://manual',
                                name: 'AI MCP Reference Manual',
                                mimeType: 'text/markdown',
                                description: 'Complete reference guide for all memory, UE4 reflection, and file transfer tools'
                            }
                        ]
                    }
                };

            case 'resources/read': {
                const uri = params?.uri;
                if (uri === 'android://manual') {
                    const manualPath = path.join(__dirname, '../../AI_MCP_MANUAL.md');
                    const text = fs.existsSync(manualPath) ? fs.readFileSync(manualPath, 'utf8') : '';
                    return {
                        jsonrpc: '2.0',
                        id,
                        result: {
                            contents: [
                                {
                                    uri: 'android://manual',
                                    mimeType: 'text/markdown',
                                    text
                                }
                            ]
                        }
                    };
                }
                return {
                    jsonrpc: '2.0',
                    id,
                    error: { code: -32602, message: `Resource not found: ${uri}` }
                };
            }

            case 'prompts/list':
                return {
                    jsonrpc: '2.0',
                    id,
                    result: {
                        prompts: [
                            {
                                name: 'analyze_android_game',
                                description: 'System prompt instructing the AI how to analyze and reverse engineer Android game memory using Root daemon tools'
                            }
                        ]
                    }
                };

            case 'prompts/get': {
                const promptName = params?.name;
                if (promptName === 'analyze_android_game') {
                    const manualPath = path.join(__dirname, '../../AI_MCP_MANUAL.md');
                    const text = fs.existsSync(manualPath) ? fs.readFileSync(manualPath, 'utf8') : '';
                    return {
                        jsonrpc: '2.0',
                        id,
                        result: {
                            description: 'Workflow instructions for reversing Android game memory',
                            messages: [
                                {
                                    role: 'user',
                                    content: {
                                        type: 'text',
                                        text: `You are connected to an Android Root Memory Inspection Daemon. Here is your operational manual:\n\n${text}`
                                    }
                                }
                            ]
                        }
                    };
                }
                return {
                    jsonrpc: '2.0',
                    id,
                    error: { code: -32602, message: `Prompt not found: ${promptName}` }
                };
            }

            case 'tools/list':
                return {
                    jsonrpc: '2.0',
                    id,
                    result: {
                        tools: MCP_TOOLS
                    }
                };

            case 'tools/call': {
                const toolName = params?.name;
                const toolArgs = params?.arguments || {};
                try {
                    const result = await this.handleCallTool(toolName, toolArgs);
                    return {
                        jsonrpc: '2.0',
                        id,
                        result
                    };
                } catch (err) {
                    return {
                        jsonrpc: '2.0',
                        id,
                        result: {
                            isError: true,
                            content: [{ type: 'text', text: `Execution failed: ${err.message}` }]
                        }
                    };
                }
            }

            case 'ping':
                return { jsonrpc: '2.0', id, result: {} };

            default:
                return {
                    jsonrpc: '2.0',
                    id,
                    error: { code: -32601, message: `Method not found: ${method}` }
                };
        }
    }
}

// If run directly from CLI / MCP runner
if (require.main === module) {
    const server = new McpServer();
    server.startStdio();
}

module.exports = { McpServer, MCP_TOOLS };
