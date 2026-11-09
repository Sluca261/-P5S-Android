# Avaliação técnica: Persona 5 Strikers no Android

Data da avaliação: 2026-11-09.

## Estado real do projeto

O aplicativo deste repositório ainda é um renderizador Vulkan de diagnóstico com controles de toque/físicos e seleção de URI NSP/XCI. A seleção do arquivo não abre o contêiner, não lê NCA/ExeFS/RomFS, não usa chaves e não executa código do Switch. A renderização de um cubo/modelo OBJ não demonstra compatibilidade com um jogo.

## Referência técnica examinada

Foi examinada a documentação pública do projeto `Brandon04Gonzalez/p5r-android` (licença MIT declarada no repositório). A arquitetura descrita inclui:
- leitura de NSP/XCI com chaves fornecidas pelo próprio usuário;
- descriptografia sob demanda dos dados do jogo;
- carregador ARM64/NCE para executar o código AArch64 do jogo;
- implementações de serviços do sistema Switch;
- implementação de NVN sobre Vulkan e tradução de shaders Maxwell para SPIR-V;
- áudio, vídeo, entrada e persistência.

Essa referência é específica para Persona 5 Royal e para o runtime de Super Mario Bros. Wonder Recomp. Ela não é um runtime genérico garantidamente compatível com Persona 5 Strikers. Não copiar ou integrar componentes sem conferir licença, dependências, avisos de terceiros e compatibilidade com o título.

## Compatibilidade ainda não demonstrada

Para Persona 5 Strikers ainda precisamos confirmar, com metadados extraídos localmente pelo usuário:
1. Title ID, versão/revisão e integridade do NSP base.
2. Estrutura de conteúdo e disponibilidade de ExeFS/RomFS.
3. ABI e requisitos do executável ARM64, bibliotecas e serviços Switch utilizados.
4. Perfil de shaders e recursos NVN, extensões Vulkan necessárias e comportamento da Adreno 619.
5. Requisitos de memória, streaming, áudio/vídeo e estabilidade em cenas reais.

O fato de o NSP ser a versão base sem atualização e de o dispositivo usar Android 13/ARM64/Vulkan é útil, mas não prova compatibilidade.

## Limites do dispositivo e meta de desempenho

Dispositivo informado: Redmi Note 11 Pro 5G, Snapdragon 695, Adreno 619, 6 GB RAM, Android 13. A meta de 30 FPS deve ser tratada como objetivo experimental, não como promessa. Um projeto de referência para outro jogo reporta desempenho abaixo de 30 FPS em cenas 3D mesmo em um aparelho Snapdragon 685 com Adreno 610 e 8 GB RAM; isso não permite prever diretamente o desempenho de Strikers.

## Plano incremental

### Fase A — preparação e identificação
- Criar no Android um fluxo local para selecionar NSP e arquivo de chaves.
- Manter as chaves no armazenamento privado do app; nunca registrá-las em logs, Git ou rede.
- Validar acesso, tamanho e metadados do arquivo sem enviar conteúdo para servidores.
- Exibir erros claros quando faltarem chaves, espaço ou permissões.

### Fase B — carregador e formatos
- Avaliar uma implementação licenciada de NSP/NCA e descriptografia.
- Confirmar que o executável e as dependências do Strikers podem ser carregados em ARM64.
- Não apresentar um botão "Jogar" funcional até que exista um loader e serviços compatíveis.

### Fase C — runtime
- Implementar/adaptar os serviços do Switch exigidos especificamente pelo jogo.
- Implementar NVN sobre Vulkan e tradução de shaders; testar extensões e memória na Adreno 619.
- Integrar áudio, vídeo, entrada, arquivos e salvamento.

### Fase D — Alpha e perfil de desempenho
- Primeiro marco: abrir o executável e produzir log técnico sem travar.
- Segundo marco: alcançar a primeira cena renderizada.
- Terceiro marco: áudio, controles e salvamento básicos.
- Quarto marco: medir FPS, memória e estabilidade; testar presets de baixa resolução/efeitos reduzidos.

## Privacidade e uso

O usuário deve utilizar apenas uma cópia obtida legalmente e as chaves extraídas do próprio console. NSP e chaves não devem ser enviados ao chat, commitados no GitHub ou incorporados ao APK. O APK distribuível deve conter somente código do runtime e componentes permitidos pelas respectivas licenças.

## Próxima ação técnica

Antes de implementar um parser ou loader específico, obter somente metadados não sensíveis do dump no próprio aparelho: título/Title ID, versão, tamanho e resultado da validação do contêiner. Nunca solicitar ou registrar o conteúdo de `prod.keys`.
