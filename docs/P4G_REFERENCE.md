# Referência arquitetural: port Android de Persona 4 Golden

Este documento registra uma análise de arquitetura pública do projeto [nxdevxz/p4g-android](https://github.com/nxdevxz/p4g-android). A referência é conceitual: este repositório não copia código, bibliotecas, assets, chaves ou arquivos proprietários de Persona 4 Golden.

## 1. Fluxo de preparação de jogo

O port de referência não baixa o jogo pela internet. O usuário fornece uma cópia própria em NSP ou XCI e o arquivo de chaves da própria consola. O launcher usa o seletor de documentos do Android, guarda a permissão de leitura do URI e mantém as chaves no armazenamento privado do aplicativo.

O fluxo observado é:

1. Selecionar um arquivo NSP/XCI local.
2. Selecionar `prod.keys` e, quando necessário, `title.keys`.
3. Validar os arquivos e iniciar uma etapa de preparação local.
4. Exibir progresso e permitir cancelamento seguro.
5. Preparar os dados e os shaders necessários.
6. Só então habilitar a opção de iniciar o jogo.

O P4G de referência possui um importador nativo específico para NSP/XCI, NCA e dados do Switch. Seu runtime também implementa execução ARM64 e compatibilidade com serviços do Switch e NVN sobre Vulkan. Portanto, copiar apenas a tela de seleção de arquivos não faria o P5S funcionar: seria necessário desenvolver ou integrar, respeitando licenças, um importador e um runtime compatíveis com a versão escolhida de Persona 5 Strikers.

**Decisão para P5S Android:** seguir o fluxo de seleção local e preparação offline, sem hospedar, fornecer ou baixar jogos e chaves. O usuário deverá indicar seu próprio dump. O aplicativo não deve declarar o jogo como preparado até que a validação e o processamento reais estejam implementados.

## 2. Controles de toque observados

O editor de controles do P4G permite mover, redimensionar, ocultar e restaurar os elementos. A disposição padrão em paisagem contempla:

- Analógico virtual esquerdo para movimentação.
- Área de arraste à direita para controle da câmera.
- Botões ABXY.
- Direcional digital (cima, baixo, esquerda e direita).
- L, ZL, R e ZR.
- − e +.
- L3 e R3.

O editor permite mover grupos (ABXY e direcional), ajustar elementos individualmente, ocultar botões e salvar o layout em configuração persistente. O port também prevê controles físicos e comportamento de retorno ao menu com confirmação.

**Decisão para P5S Android:** adotar esse conjunto como perfil inicial, mas adaptar as ações à disposição e às necessidades de Persona 5 Strikers. O mapeamento final precisa ser validado no jogo; não se deve presumir que cada ação de P4G corresponda à mesma ação em P5S.

## 3. Arquitetura necessária para o projeto atual

O projeto P5S atual é um renderer Vulkan de diagnóstico com carregamento OBJ/MTL. Ele ainda não contém o executável ARM64 do jogo, um loader Horizon, serviços do Switch, tradução NVN, áudio, vídeo ou importador NSP/XCI.

A evolução precisa ser dividida em componentes independentes:

- **Launcher Android:** seleção persistente do dump, seleção privada das chaves, validação, progresso, cancelamento e diagnóstico.
- **Importador:** leitura de NSP/XCI e estruturas NCA/ExeFS/RomFS, com tratamento correto de criptografia e revisões suportadas. Nenhum segredo deve ser registrado em logs.
- **Runtime:** loader ARM64, serviços do sistema Switch e compatibilidade gráfica NVN/Vulkan, além de áudio, vídeo, temporização e entrada. Este é o maior bloqueio técnico.
- **Entrada:** camada comum para gamepad Android e controles de toque, com layout configurável.
- **Persistência:** saves e configurações locais, com cópias de segurança antes de operações destrutivas.

## 4. Próximas etapas recomendadas

1. Construir o launcher e a seleção segura de NSP/XCI e chaves.
2. Implementar e testar a camada de entrada Android para gamepads e controles de toque, ainda sem afirmar que controla o jogo.
3. Investigar o formato e a revisão exata do dump de Persona 5 Strikers para Switch.
4. Avaliar a viabilidade de um runtime compatível; não tratar o renderer de diagnóstico como substituto desse runtime.
5. Só depois ligar a preparação real ao botão de iniciar.

## 5. Segurança e direitos autorais

O aplicativo deve aceitar somente arquivos fornecidos pelo usuário e não deve oferecer links para downloads de jogos ou chaves. Dumps, chaves e conteúdo do jogo não devem ser enviados a repositórios públicos, logs ou artefatos de CI. A compatibilidade deve ser limitada a revisões efetivamente testadas.
