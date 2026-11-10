# P5S Android

Projeto experimental de pesquisa para avaliar a viabilidade técnica de um port nativo de Persona 5 Strikers para Android.

## Estado atual — 10/11/2026

O projeto já integra ao APK Android um runtime ARM64 compartilhado, compilado com execução NCE, carregamento de contêiner NSP/XCI, suporte a chaves locais, backend NVN/Vulkan e tradutor Maxwell→SPIR-V sob demanda. O launcher encaminha o descritor do arquivo e os caminhos privados das chaves ao NativeActivity. O runtime também valida o Title ID-base de Persona 5 Strikers, tenta carregar módulos opcionais `subsdk1` a `subsdk9` e registra o Build ID do executável e uma amostra limitada de importações não resolvidas para orientar a compatibilidade.

A compilação integrada mais recente, [42795817189](https://github.com/Sluca261/-P5S-Android/actions/runs/42795817189), no commit `85655e6c9b952aad47217ddfd5c2b01a12c34029`, foi concluída com sucesso. Além de compilar o runtime ARM64 e o tradutor NativeAOT, o APK passou pela verificação estática das três bibliotecas ARM64, do ponto de entrada NativeActivity, do export do tradutor e da ponte JNI. O commit também serializa as traduções de shaders para proteger os arquivos temporários e o estado interno do tradutor contra chamadas simultâneas. Isso comprova build, empacotamento e símbolos, **não** a execução do jogo.

A tela e o modelo Vulkan de diagnóstico continuam disponíveis para testes independentes. Eles não representam gráficos, cenas ou controles do Persona 5 Strikers. O adaptador específico de P5S ainda não possui hooks ou offsets de jogo validados. A próxima etapa é obter evidências de inicialização com uma cópia local do jogo, identificar incompatibilidades por meio dos logs e avançar até uma cena real e combate interativo.

**O projeto ainda não é uma versão jogável de Persona 5 Strikers.** Não há comprovação de boot completo, primeira cena, combate, áudio ou salvamento.

O workflow `Android launcher-only debug APK` gera apenas um APK do launcher/diagnóstico, sem as bibliotecas do runtime integrado. Seu artefato é identificado como `P5S-Android-launcher-only-debug`. Para avaliar a integração nativa, considere somente o artefato `P5S-Android-runtime-debug` publicado pelo workflow `P5S shared runtime integration`; mesmo esse APK ainda não foi validado em execução real do jogo.

## Limites e uso responsável

- O repositório não inclui assets, executáveis, músicas ou código proprietário do jogo.
- O teste Vulkan e a renderização 3D não comprovam que o jogo completo possa ser executado no aparelho.
- Qualquer futura análise ou importação de arquivos deve utilizar cópias obtidas legalmente pelo próprio usuário e respeitar os direitos autorais e as licenças aplicáveis.

## Entrada unificada: toque e controle físico (etapa 21)

Os botões virtuais e os eventos de controles físicos agora alimentam a mesma classe `GameInputState`. Ela mantém um bitmask de botões (A/B/X/Y, L/R, ZL/ZR, START/SELECT e direcional) e quatro eixos normalizados para os analógicos esquerdo e direito. Os eixos aplicam limite de -1 a 1 e uma zona morta central de 0,12; o analógico direito aceita os pares RX/RY e, como alternativa, Z/RZ.

**Limitação atual:** esta camada unifica e apresenta os dados de entrada para diagnóstico, mas ainda não envia comandos a um motor de jogo. A integração com um runtime depende da implementação futura desse runtime e do seu contrato de entrada.

## Ponte JNI e resposta visual aos analógicos (etapa 22)

A classe Java encaminha por JNI o bitmask de botões e os quatro eixos normalizados para o lado nativo. O renderizador Vulkan lê os eixos de forma thread-safe: o analógico direito altera a orientação do modelo de diagnóstico e o esquerdo desloca sua posição na área de teste. A rotação automática foi desacelerada para deixar a resposta aos comandos mais perceptível.

Os botões já atravessam a ponte e ficam disponíveis no estado nativo para a futura camada de runtime, mas ainda não executam ações de jogo. A resposta visual atual é somente um teste da integração de entrada com a renderização, não controle de Persona 5 Strikers.

## Próximos marcos

1. Validar a camada unificada com controles físicos diferentes e múltiplos toques.
2. Definir uma interface JNI estável para encaminhar estados de entrada ao futuro runtime.
3. Pesquisar, de forma independente, formatos e requisitos técnicos de importação de recursos.
4. Medir desempenho, memória e compatibilidade no dispositivo Android de teste.


## Launcher: seleção local de NSP/XCI (etapa inicial)

O launcher Android permite selecionar uma cópia local de jogo em formato `.nsp` ou `.xci` pelo seletor de documentos do sistema. Ele guarda a permissão persistente de leitura do URI, registra o nome do arquivo e verifica se o acesso continua disponível ao abrir o aplicativo. O arquivo não é copiado para o armazenamento do app nem enviado pela rede.

**Limitação atual:** esta é somente a primeira camada do launcher. O projeto ainda não interpreta contêineres NSP/XCI, não descriptografa NCA, não importa chaves, não instala conteúdo e não executa o jogo. A mensagem de estado do aplicativo deixa essa limitação explícita. Utilize apenas dumps obtidos legalmente pelo próprio usuário; não compartilhe jogos ou chaves no repositório.


## Interpretação nativa dos botões (etapa 23)

A atividade Java encaminha o estado completo de entrada à função JNI `updateInputState`, que valida os quatro eixos e armazena o bitmask de botões em variáveis atômicas. No renderizador de diagnóstico, A e B alteram o sentido da rotação automática, L e R acrescentam rotação horizontal e o direcional desloca o modelo. Os analógicos continuam controlando deslocamento e orientação. Isso valida o caminho de entrada até a camada nativa/Vulkan; ainda não representa ações de Persona 5 Strikers nem integração com um runtime de jogo.


## Eventos de entrada e comandos de borda (etapa 24)

A camada unificada agora diferencia o estado mantido de cada botão dos eventos de pressionar e soltar. Cada transição válida recebe uma sequência crescente; repetições do mesmo estado não geram eventos duplicados. A ponte JNI encaminha o bitmask atual, as máscaras de eventos e a sequência ao código nativo. O renderizador consome os eventos de pressionamento uma única vez: X e Y aplicam pequenos ajustes acumulativos à orientação do modelo de diagnóstico. Isso valida o ciclo de eventos e a comunicação Java/JNI/Vulkan, sem representar comandos do jogo real.


## Avaliação de viabilidade do port nativo

A análise de arquitetura, requisitos ainda não validados e etapas necessárias para uma Alpha executável está documentada em [`docs/PORT_FEASIBILITY.md`](docs/PORT_FEASIBILITY.md). O projeto permanece experimental: ainda não carrega nem executa o Persona 5 Strikers.


## Auditoria de integração do runtime P5R

A auditoria da arquitetura de referência, dos pontos específicos de Persona 5 Royal e das diferenças de build está em [`docs/P5R_RUNTIME_AUDIT.md`](docs/P5R_RUNTIME_AUDIT.md). O runtime de referência não é um drop-in: P5S exige adaptador e importador próprios; os hooks e validadores específicos de P5R não podem ser reutilizados sem análise.


## Identificação do executável-alvo (etapa de preparação)

Foi adicionada uma definição nativa isolada para reconhecer o Title ID base de Persona 5 Strikers (`0100801011C3E000`) e registrar o Build ID de referência associado à versão 1.0.0 (`C4DF04F647BDC727`). A função de identificação aceita também IDs relacionados cujo sufixo de 12 bits represente conteúdo/atualização. Esses valores são referências iniciais de catálogo e ainda precisam ser confrontados com os metadados do arquivo local do usuário. A tela de diagnóstico apenas exibe os identificadores conhecidos: ela **não lê o NSP/XCI, não confirma a versão instalada e não executa o jogo**.


## Adaptador inicial do runtime para a versão americana

O contrato mínimo do jogo para uma futura integração do runtime compartilhado está em `runtime-adapter/games/p5s/game.cpp`. Ele registra o Title ID-base americano e mantém deliberadamente vazios os callbacks específicos, sem importar offsets ou hooks de Persona 5 Royal. A integração ao runtime compartilhado já é feita pelo workflow de CI, mas os callbacks específicos continuam vazios e **o APK ainda não é jogável**. Os requisitos de integração e validação estão em [P5S_US_RUNTIME_INTEGRATION.md](docs/P5S_US_RUNTIME_INTEGRATION.md).

## Integração do runtime ARM64 (2026-11-10)

A integração experimental do runtime compartilhado foi conectada ao APK: o CI compila `libp5s.so` com NCE, gera o tradutor ARM64 Maxwell→SPIR-V e empacota as bibliotecas no aplicativo. A execução mais recente, [42795817189](https://github.com/Sluca261/-P5S-Android/actions/runs/42795817189), passou pela validação estática de arquitetura ELF e dos símbolos NativeActivity, tradutor e ponte JNI.

**Importante:** isso comprova build e empacotamento, não compatibilidade nem jogabilidade. Ainda não há comprovação de boot de um dump real de Persona 5 Strikers, primeira cena renderizada, combate, áudio ou salvamento. O adaptador P5S permanece sem hooks específicos até que o executável correto seja identificado e analisado. Consulte [docs/P5S_US_RUNTIME_INTEGRATION.md](docs/P5S_US_RUNTIME_INTEGRATION.md) para os marcos e limitações atuais.

## Validação nativa do contêiner no descritor do runtime (etapa 27)

A ponte JNI agora valida o próprio descritor que será entregue ao runtime, em vez de confiar apenas no nome do arquivo ou em uma leitura separada pelo Storage Access Framework. Antes de configurar o ambiente nativo, exige um arquivo regular, legível e seekable; em seguida, usa `pread()` para conferir a assinatura `PFS0` no início de NSP ou `HEAD` no deslocamento `0x100` de XCI. A comparação da extensão ignora maiúsculas/minúsculas.

Essa checagem reduz o risco de iniciar o runtime com descritor não suportado, arquivo trocado após a seleção ou extensão que não corresponde ao cabeçalho. Ela não interpreta NCA, não descriptografa conteúdo, não confirma Title ID/Build ID e não comprova boot do jogo. A próxima validação depende da compilação do workflow de runtime e de um teste no dispositivo com um arquivo local obtido legalmente.

## Primeiro frame do runtime no dispositivo (10/11/2026)

Dois testes no aparelho alcançaram a inicialização de NVN/Vulkan, apresentação do primeiro frame, inicialização do renderer de áudio e tradução de shaders. A execução termina logo depois, no entorno de uma chamada HID, com resultado `0x52CA`. A auditoria diferencia a função HLE de toque (presente, mas retorna zero contatos) do serviço IPC `hid` (ainda não registrado e atendido por stub). A causa exata do encerramento ainda está em investigação; também há pendências separadas no mapeamento de recursos de shaders e em formatos de textura/Vulkan. Consulte [o relatório de diagnóstico](docs/DEVICE_BOOT_DIAGNOSTICS_2026-11-10.md).

**Esse marco comprova apresentação de um frame pelo runtime, não uma cena do jogo nem jogabilidade. O projeto continua experimental e não jogável.** O workflow integrado [run 42804446224](https://github.com/Sluca261/-P5S-Android/actions/runs/42804446224) foi concluído com sucesso e publicou o APK `P5S-Android-runtime-debug`.

### Reprodutibilidade e decodificação do erro HID — terceira execução

A terceira execução no mesmo aparelho repetiu o mesmo encerramento observado nas duas anteriores. O resultado `0x52CA` foi decodificado conforme o formato Horizon: módulo = bits inferiores de 9 bits (`202`, HID) e descrição = bits restantes (`41`), equivalente a `2202-0041`. A descrição 41 ainda não foi identificada. A repetição e a classificação do módulo fortalecem a hipótese de bloqueio HID, mas ainda é necessário identificar o comando IPC/rotina exata que retorna o erro. Os endereços da vi layer e da cópia buffer-to-texture mudaram sem alterar o resultado. Detalhes em [DEVICE_BOOT_DIAGNOSTICS_2026-11-10.md](docs/DEVICE_BOOT_DIAGNOSTICS_2026-11-10.md).
