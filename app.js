// ============================================================
//  LINX_MCP_LECTOR - Web Landing Page Logic
//  Powered by: uam.lol/c
// ============================================================

const TOOLS_DATA = {
    ue4: [
        {
            name: "mem_ue4_roots",
            desc: "Resuelve las raíces globales del motor UE4 (lib_base, FNamePool, GUObjectArray, GWorld) en tiempo real."
        },
        {
            name: "mem_get_world_actors",
            desc: "Extrae el estado de la partida: lista de jugadores, bots, objetos de loot y coordenadas 3D (X, Y, Z)."
        },
        {
            name: "mem_inspect_actor",
            desc: "Radiografía profunda de un actor: vuelca posición exacta, rotación Pitch/Yaw/Roll, Mesh, Huesos y PlayerState."
        },
        {
            name: "mem_set_ue4_config",
            desc: "Hot-Tuning en vivo: modifica offsets de cámara, actores, root_comp y persistent_level sin reiniciar el juego."
        },
        {
            name: "mem_set_draw_config",
            desc: "Control en tiempo real del HUD Vulkan/ImGui (cajas 2D/3D, esqueleto, líneas, distancia, radar, loot)."
        },
        {
            name: "mem_dump_fixed_elf",
            desc: "Vuelca y repara la librería libUE4.so descifrada desde la RAM para análisis en IDA Pro y Ghidra."
        }
    ],
    memory: [
        {
            name: "mem_read_hex / types",
            desc: "Lectura de memoria en formato hexadecimal o tipado (Int8/16/32/64, Float, Double, Vec3, ASCII)."
        },
        {
            name: "mem_write_typed",
            desc: "Escritura tipada directa en RAM (modifica FOV, recoil, variables float o enteros en caliente)."
        },
        {
            name: "mem_patch",
            desc: "Aplica parches de instrucciones ARM64 (NOPs, b, ret) y almacena copia de seguridad automática."
        },
        {
            name: "mem_restore",
            desc: "Reversión / Rollback instantáneo de parches de memoria a su estado original."
        },
        {
            name: "mem_read_pointer_chain",
            desc: "Sigue y resuelve cadenas de punteros multinivel en la memoria del juego."
        },
        {
            name: "mem_pattern_scan",
            desc: "Escáner AOB de firmas de bytes con comodines (??) en regiones de memoria o módulos .so."
        }
    ],
    process: [
        {
            name: "mem_status",
            desc: "Consulta estado de la conexión, latencia ping, modo (USB/Wi-Fi), juego vinculado y PID."
        },
        {
            name: "mem_attach",
            desc: "Vincula el daemon a un paquete específico (ej: com.proximabeta.mf.liteuamo) o PID numérico."
        },
        {
            name: "mem_list_processes",
            desc: "Lista todos los procesos activos en Android y resalta automáticamente juegos conocidos."
        },
        {
            name: "mem_get_modules",
            desc: "Enumera módulos y librerías .so cargadas en la memoria con su dirección base y tamaño."
        }
    ],
    files: [
        {
            name: "mem_fs_list",
            desc: "Explora directorios remotos en el celular (/data/local/tmp/) desde la PC o mediante la IA."
        },
        {
            name: "mem_file_download",
            desc: "Descarga archivos desde el móvil a la computadora a través de streaming TCP por el socket."
        },
        {
            name: "mem_compress_archive",
            desc: "Comprime archivos o carpetas completas en formato ZIP o TAR directamente en el teléfono."
        },
        {
            name: "mem_push_auto_update",
            desc: "Hot-Reload del binario mem_server.sh en el celular sin reiniciar el juego ni desconectar el socket."
        }
    ]
};

document.addEventListener('DOMContentLoaded', () => {
    // Populate Initial Tools
    renderTools('ue4');

    // Tool Category Tabs
    const toolTabs = document.querySelectorAll('.tool-tab');
    toolTabs.forEach(tab => {
        tab.addEventListener('click', () => {
            toolTabs.forEach(t => t.classList.remove('active'));
            tab.classList.add('active');
            const category = tab.getAttribute('data-category');
            renderTools(category);
        });
    });

    // Architecture Nodes Interaction
    const archNodes = document.querySelectorAll('.arch-node');
    archNodes.forEach(node => {
        node.addEventListener('click', () => {
            archNodes.forEach(n => n.classList.remove('active'));
            node.classList.add('active');
        });
    });
});

function renderTools(category) {
    const container = document.getElementById('toolsDisplay');
    if (!container) return;

    const tools = TOOLS_DATA[category] || [];
    container.innerHTML = tools.map(tool => `
        <div class="tool-item">
            <div class="tool-item-name">${escapeHtml(tool.name)}</div>
            <div class="tool-item-desc">${escapeHtml(tool.desc)}</div>
        </div>
    `).join('');
}

function escapeHtml(str) {
    return str.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
}
