# Project Finish Checklist

## 1. Instalar pré-requisitos
- [x] Instalar CMake
- [x] Instalar Clang/LLVM
- [x] Instalar x86_64-elf-binutils ou linker compatível
- [x] Instalar o compilador cruzado x86_64-w64-mingw32
- [x] Instalar QEMU
- [x] Instalar a firmware OVMF/EDK2 para boot UEFI
- [x] Garantir que os scripts tenham permissão de execução: `chmod +x scripts/*.sh`

## 2. Build do projeto
- [x] Rodar o setup de ambiente: `./scripts/setup_step1.sh`
- [x] Rodar o build: `./scripts/setup_step2.sh`
- [x] Confirmar que os artefatos foram gerados:
  - [x] `build/bootloader/BOOTX64.EFI.exe`
  - [x] `build/kernel/kernel.elf`

## 3. Rodar o projeto
- [x] Rodar o launcher: `./scripts/setup_step3.sh`
- [x] Confirmar que o QEMU iniciou com a firmware UEFI
- [x] Confirmar que a ESP foi montada como `FS0:`
- [x] Confirmar que o bootloader foi encontrado em `EFI/BOOT/BOOTX64.EFI`
- [x] Confirmar que o bootloader foi executado
- [x] Corrigir abertura do diretório raiz `/`
- [x] Corrigir inicialização do GOP/Framebuffer
- [x] Confirmar leitura de `kernel/kernel.elf`
- [x] Confirmar que o kernel foi carregado a partir da ESP

## Roadmap do projeto

### Fase 1 — UEFI Boot
- [x] Instalar as ferramentas de build e emulação
- [x] Configurar o ambiente de compilação
- [x] Compilar o bootloader UEFI
- [x] Compilar o kernel
- [x] Gerar a imagem EFI/ESP
- [x] Executar o bootloader no QEMU
- [x] Carregar o kernel pelo bootloader

### Fase 2 — Hardware Initialization
- [x] Implementar Memory Map
- [x] Integrar Framebuffer
- [x] Estudar e adicionar ACPI
- [x] Coletar informações via CPUID
- [x] Criar estrutura de BootInfo

### Fase 3 — Kernel
- [x] Inicializar o kernel corretamente
- [x] Implementar console de saída
- [x] Adicionar sistema de logging
- [x] Criar mecanismo de kernel panic

### Fase 4 — Gerenciamento de Memória
- [x] Implementar Physical Memory Manager
- [x] Implementar Virtual Memory
- [x] Adicionar Paging
- [x] Criar Kernel Heap

### Fase 5 — CPU
- [x] Configurar GDT
- [x] Configurar IDT
- [x] Implementar TSS
- [x] Tratar exceptions

### Fase 6 — Scheduler
- [x] Implementar threads
- [x] Implementar processos
- [x] Criar context switch
- [x] Adicionar Round Robin

### Fase 7 — Drivers
- [x] Implementar driver de framebuffer
- [x] Implementar teclado
- [x] Implementar mouse
- [x] Adicionar suporte a PCI
- [x] Adicionar suporte a USB
- [x] Adicionar suporte a NVMe
- [x] Adicionar suporte a rede

### Fase 8 — Sistema de Arquivos
- [x] Implementar VFS
- [x] Adicionar suporte a FAT32
- [x] Adicionar suporte a EXT2

### Fase 9 — Userspace
- [x] Implementar system calls
- [x] Criar shell
- [x] Rodar processos em modo usuário

### Fase 10 — Interface Gráfica
- [x] Implementar window manager
- [x] Implementar compositor
- [x] Criar desktop environment
- [x] Criar fila de eventos da GUI
- [x] Adicionar tipos de evento `MouseMove`, `MouseDown`, `MouseUp`, `Click`, `DoubleClick`, `Hover` e `Drag`
- [x] Renderizar cursor de mouse
- [x] Implementar clique lógico na GUI
- [x] Implementar arraste de janelas pela barra de título
- [x] Implementar z-order com bring-to-front ao clicar em janela
- [x] Adicionar barra de título com nome da janela
- [x] Adicionar botão de fechar na barra de título
- [x] Adicionar janela visual de Terminal
- [x] Exibir status visual de CPU, RAM, FPS e relógio na barra superior
- [x] Criar API gráfica interna para retângulos, linhas, texto e imagem placeholder
- [x] Adicionar wallpaper, barra inferior e botões de taskbar para janelas abertas
- [x] Adicionar estados visuais ativos/inativos para janelas e controles
- [ ] Conectar pacotes reais do mouse PS/2 à fila de eventos da GUI
- [x] Implementar hover com estado visual
- [ ] Implementar double click
- [x] Implementar minimizar janela
- [x] Implementar maximizar/restaurar janela
- [x] Adicionar ícones no desktop
- [ ] Implementar comandos reais do terminal: `help`, `mem`, `cpu`, `clear`, `version`, `uptime`, `reboot`

### Fase 11 — Rede
- [ ] Implementar Ethernet
- [ ] Implementar IPv4
- [ ] Implementar TCP
- [ ] Implementar UDP
- [ ] Implementar DHCP
- [ ] Implementar DNS

## Prerequisites
- [x] Install CMake
- [x] Install Clang/LLVM
- [x] Install x86_64-elf-binutils or compatible linker
- [x] Install x86_64-w64-mingw32 cross-compiler
- [x] Install QEMU
- [x] Install OVMF/EDK2 firmware image for UEFI boot

## Build
- [x] Make the helper scripts executable: `chmod +x scripts/build.sh scripts/run.sh`
- [x] Run the build script: `./scripts/build.sh`
- [x] Confirm build artifacts exist:
  - [x] `build/bootloader/BOOTX64.EFI.exe`
  - [x] `build/kernel/kernel.elf`

## Run
- [x] Run the boot script: `./scripts/run.sh`
- [x] Confirm QEMU launches with the UEFI firmware image
- [x] Confirm the ESP is exposed as the boot volume
- [x] Confirm the bootloader is loaded from `EFI/BOOT/BOOTX64.EFI`
- [x] Confirm `/kernel/kernel.elf` is opened and read from the ESP
- [x] Confirm the kernel ELF is loaded by the bootloader

## Verification
- [x] Check that `ESP/EFI/BOOT/BOOTX64.EFI` exists
- [x] Check that `ESP/kernel/kernel.elf` exists
- [x] Verify the project builds without errors
- [x] Verify GOP/framebuffer initialization succeeds
- [x] Verify framebuffer colors/text render with the detected pixel format
- [x] Verify `ExitBootServices` uses a fresh memory map key

## Troubleshooting
- [ ] If the build fails, verify the toolchain paths in the CMake files
- [ ] If QEMU fails, verify OVMF firmware is installed and discoverable
- [ ] If the bootloader does not start, inspect the generated EFI binary and kernel ELF
