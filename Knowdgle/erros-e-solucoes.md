# Knowdgle — Erros e Soluções

Este arquivo reúne erros comuns que podem aparecer ao compilar, rodar ou ajustar este projeto, junto com possíveis soluções.

## 1. Erro de ferramenta não encontrada
- **Sintoma:** `cmake: command not found`, `clang: command not found`, `qemu-system-x86_64: command not found`
- **Solução:** instalar as ferramentas necessárias no sistema, como CMake, Clang/LLVM, QEMU e o compilador cruzado apropriado.

## 2. Falha no build do bootloader
- **Sintoma:** erro ao gerar `BOOTX64.EFI`
- **Solução:** verificar se o compilador MinGW foi instalado e se os caminhos estão corretos nos arquivos CMake.

## 3. Falha no build do kernel
- **Sintoma:** erro ao gerar `kernel.elf`
- **Solução:** verificar se `x86_64-elf-ld` e o Clang estão instalados e acessíveis.

## 4. Arquivo OVMF não encontrado
- **Sintoma:** `Error: OVMF (EDK2) UEFI firmware image not found.`
- **Solução:** instalar o pacote de firmware UEFI do QEMU/OVMF e confirmar o caminho esperado pelo script.

## 5. Script de build não executa
- **Sintoma:** permissão negada ao executar `./scripts/build.sh`
- **Solução:** dar permissão com `chmod +x scripts/build.sh scripts/run.sh`.

## 6. Arquivos de build ausentes
- **Sintoma:** `Build artifacts missing. Triggering build...`
- **Solução:** rodar o build novamente e verificar se os artefatos foram gerados corretamente.

## 7. Erro ao criar a imagem EFI
- **Sintoma:** falha ao gerar `esp.img`
- **Solução:** verificar se `hdiutil` (macOS) ou `mtools`/`mkfs.vfat` (Linux) estão instalados.

## 8. QEMU não inicia corretamente
- **Sintoma:** tela em branco ou falha de inicialização
- **Solução:** confirmar se o firmware OVMF foi localizado e se os arquivos EFI e kernel foram copiados para a estrutura correta.

## 9. Erro após alterar código do bootloader
- **Sintoma:** o projeto compila, mas o comportamento muda de forma inesperada
- **Solução:** limpar build anterior, recompilar e testar novamente.

## 10. Erro após alterar código do kernel
- **Sintoma:** o kernel não é carregado ou falha na execução
- **Solução:** revisar o ponto de entrada, links e os offsets de memória usados no projeto.
