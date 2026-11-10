# Diagnóstico de execução no dispositivo — 10/11/2026

## Evidência observada

Em três testes no mesmo aparelho, o runtime avançou além do carregamento inicial do NSP e executou a inicialização do programa (`nn::init::Start` / `nnMain`). Os logs indicaram:

- NSP aberto diretamente, com Title ID `0100801011c3e000`, 174 arquivos RomFS e 5 arquivos ExeFS;
- módulos principais e SDK carregados, com Build ID registrado;
- inicialização do NVN sobre Vulkan 1.1 no Adreno 619;
- criação de swapchain de 918 × 1964 e apresentação do primeiro frame;
- inicialização do renderer de áudio;
- tradução de dois shaders Maxwell.

Isso representa o primeiro avanço verificável até a apresentação de um frame pelo runtime no dispositivo. **Não comprova que o frame seja uma cena do jogo**: ainda é necessário distinguir conteúdo de diagnóstico/renderização do runtime de uma cena real de Persona 5 Strikers.

## Bloqueio observado

A terceira execução, no mesmo aparelho e pelo mesmo caminho, reproduziu o comportamento das duas anteriores: primeiro frame e encerramento cerca de 1,2 segundo depois, no entorno da chamada `nn::hid::GetTouchScreenState`, com resultado `0x52CA` e endereço de retorno `main+4d58fc`. A decodificação Horizon usa os 9 bits inferiores para o módulo e os bits seguintes para a descrição: `0x52CA & 0x1FF = 202` (módulo HID) e `0x52CA >> 9 = 41` (descrição 41). Em formato de erro, isso corresponde a `2202-0041`. O módulo é, portanto, HID; a semântica específica da descrição 41 ainda não foi identificada.

A auditoria do runtime de referência encontrou duas camadas diferentes:

1. A função HLE `GetTouchScreenState` existe em `runtime/src/hle_hid.cpp`, mas atualmente informa zero contatos de toque.
2. O serviço IPC chamado `hid` não está registrado. O dispatcher de `runtime/src/ipc.cpp` cria um serviço genérico quando não encontra uma implementação; seus comandos desconhecidos retornam sucesso vazio e geram avisos de stub.

Portanto, não é correto descrever o problema simplesmente como “a função HLE de toque está ausente”. A ausência do serviço IPC e a resposta HLE sem contatos são questões distintas. A classificação do resultado como módulo HID, somada à chamada de toque e ao serviço `hid` atendido por stub, fortalece bastante a hipótese de falha na camada HID; ainda assim, não prova qual comando ou rotina específica dispara o erro 41.

## Outras incompatibilidades registradas

- O tradutor relata ausência de `results.tsv` em uma situação na qual os buffers de recursos de shaders não podem ser associados.
- Há pelo menos um formato de textura (`0x95`) não suportado, deixando a textura sem imagem.
- O dispositivo não anuncia algumas extensões Vulkan opcionais esperadas pelo runtime.
- A orientação da superfície reportada pelo runtime difere da orientação da janela Android.
- Permanecem importações dinâmicas não resolvidas e comandos IPC sem implementação.

Esses itens devem ser tratados separadamente. Não há evidência suficiente para afirmar que sejam a causa do abort imediatamente após a chamada HID.

## Próxima sequência técnica

1. Capturar a mensagem completa associada ao resultado `0x52CA` e decodificar módulo e descrição usando as definições de resultado do runtime.
2. Correlacionar `main+4d58fc` com o Build ID exato e símbolos disponíveis para identificar a operação que falhou.
3. Registrar os nomes e IDs de comando do serviço `hid` efetivamente solicitados antes do encerramento; implementar somente o conjunto necessário, com estruturas e respostas compatíveis.
4. Criar uma ponte explícita entre o estado de toque/controle do Android e a função de leitura de controles usada pelo runtime. A entrada de diagnóstico já existente no launcher não deve ser confundida com entrada já conectada ao runtime.
5. Corrigir a tabela de recursos do tradutor e, em seguida, tratar formatos de textura e particularidades Vulkan.

## Estado

O runtime ARM64 e o APK integrado foram compilados com sucesso no workflow [P5S shared runtime integration — execução 48](https://github.com/Sluca261/-P5S-Android/actions/runs/42804446224). O APK contém as bibliotecas do runtime e do tradutor e passou pela verificação estática do pipeline. Os três testes no dispositivo alcançaram inicialização de NVN e apresentação do primeiro frame, mas terminaram de forma consistente durante a inicialização/leitura HID. Na terceira execução, os endereços observados para a vi layer (`0xb4000072b4fe2810`) e para buffer-to-texture copy (`501cb19fb0`) mudaram, sem alterar o resultado final.

**O projeto continua experimental e não é jogável.** Não há comprovação de cena jogável, controle funcional dentro do jogo, combate, áudio de jogo ou salvamento.

## Instrumentação HID preparada para a próxima compilação

Foi atualizado o script de adaptação do runtime para incluir rastreamento limitado (até 128 solicitações IPC) de chamadas ao serviço `hid`, registrando ID do comando, tipo da mensagem, tamanho dos dados e quantidade de descritores/objetos. Também serão registradas as primeiras 32 entradas na HLE `GetTouchScreenState`, com o endereço de saída e o contador de amostragem.

A instrumentação é somente observacional: não registra conteúdo sensível dos buffers, não inventa respostas, não altera o comportamento do serviço e não conecta ainda os controles Android ao runtime. O objetivo é descobrir se o processo consulta o serviço IPC `hid` antes do erro e quais IDs de comando aparecem, além de confirmar quantas vezes a rotina HLE de toque é chamada. O workflow de integração é disparado pela alteração do script; o próximo teste no aparelho deverá usar o APK resultante e compartilhar o novo `nx.log` completo.
