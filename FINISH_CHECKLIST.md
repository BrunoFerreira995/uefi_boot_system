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
- [ ] Corrigir inicialização do GOP/Framebuffer
- [ ] Confirmar leitura de `kernel/kernel.elf`
- [ ] Confirmar que o kernel foi carregado a partir da ESP

## Roadmap do projeto

### Fase 1 — UEFI Boot
- [x] Instalar as ferramentas de build e emulação
- [x] Configurar o ambiente de compilação
- [x] Compilar o bootloader UEFI
- [x] Compilar o kernel
- [x] Gerar a imagem EFI/ESP
- [x] Executar o bootloader no QEMU
- [ ] Carregar o kernel pelo bootloader

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

### Fase 11 — Rede
- [ ] Implementar Ethernet
- [ ] Implementar IPv4
- [ ] Implementar TCP
- [ ] Implementar UDP
- [ ] Implementar DHCP
- [ ] Implementar DNS

## Prerequisites
- [ ] Install CMake
- [ ] Install Clang/LLVM
- [ ] Install x86_64-elf-binutils or compatible linker
- [ ] Install x86_64-w64-mingw32 cross-compiler
- [ ] Install QEMU
- [ ] Install OVMF/EDK2 firmware image for UEFI boot

## Build
- [ ] Make the helper scripts executable: `chmod +x scripts/build.sh scripts/run.sh`
- [ ] Run the build script: `./scripts/build.sh`
- [ ] Confirm build artifacts exist:
  - [ ] `build/bootloader/BOOTX64.EFI`
  - [ ] `build/bootloader/BOOTX64.EFI.exe`
  - [ ] `build/kernel/kernel.elf`

## Run
- [ ] Run the boot script: `./scripts/run.sh`
- [ ] Confirm QEMU launches with the UEFI firmware image
- [ ] Confirm the bootloader and kernel are loaded from the ESP image

## Verification
- [ ] Check that `ESP/EFI/BOOT/BOOTX64.EFI` exists
- [ ] Check that `ESP/kernel/kernel.elf` exists
- [ ] Verify the project boots without build errors

## Troubleshooting
- [ ] If the build fails, verify the toolchain paths in the CMake files
- [ ] If QEMU fails, verify OVMF firmware is installed and discoverable
- [ ] If the bootloader does not start, inspect the generated EFI binary and kernel ELF
