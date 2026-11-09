# P5S Android

Projeto experimental de pesquisa para avaliar a viabilidade técnica de um port nativo de Persona 5 Strikers para Android.

## Estado atual

O aplicativo possui uma base Android em Java/C++, inicialização Vulkan, apresentação contínua em uma superfície Android, pipeline gráfica 3D, buffer de profundidade, iluminação direcional, textura de teste e carregamento de shaders SPIR-V empacotados no APK.

A etapa atual amplia o carregador Wavefront OBJ para aceitar bibliotecas MTL simples com cores difusas `Kd`, grupos `usemtl`, coordenadas UV e mapas `map_Kd`. Os mapas difusos agora são carregados como imagens Vulkan independentes: cada material possui seu próprio `VkImage`, memória, `VkImageView`, `VkSampler` e entrada em um array de descritores combinados. O OBJ transporta um índice de material por vértice, e o shader seleciona a textura correspondente sem remapear as UVs para regiões de atlas. Os arquivos de textura são `ruby_texture.ppm` e `azure_texture.ppm`. O parser também aceita posições, normais, faces triangulares ou poligonais e índices positivos/negativos; triangula polígonos, calcula normais ausentes, centraliza e normaliza a escala do modelo. A leitura dos arquivos é limitada e validada. O modelo e os materiais de diagnóstico são originais e estão em `app/src/main/assets/diagnostic_model.obj` e `app/src/main/assets/diagnostic_materials.mtl`.

**Isto ainda não é um port jogável de Persona 5 Strikers.** A geometria, textura e shaders presentes são recursos de teste; não há implementação do formato de dados, motor ou conteúdo proprietário do jogo.

## Limites e uso responsável

- O repositório não inclui assets, executáveis, músicas ou código proprietário do jogo.
- O teste Vulkan e a renderização 3D não comprovam que o jogo completo possa ser executado no aparelho.
- Qualquer futura análise ou importação de arquivos deve utilizar cópias obtidas legalmente pelo próprio usuário e respeitar os direitos autorais e as licenças aplicáveis.

## Entrada unificada: toque e controle físico (etapa 21)

Os botões virtuais e os eventos de controles físicos agora alimentam a mesma classe `GameInputState`. Ela mantém um bitmask de botões (A/B/X/Y, L/R, ZL/ZR, START/SELECT e direcional) e quatro eixos normalizados para os analógicos esquerdo e direito. Os eixos aplicam limite de -1 a 1 e uma zona morta central de 0,12; o analógico direito aceita os pares RX/RY e, como alternativa, Z/RZ.

**Limitação atual:** esta camada unifica e apresenta os dados de entrada para diagnóstico, mas ainda não envia comandos a um motor de jogo. A integração com um runtime depende da implementação futura desse runtime e do seu contrato de entrada.

## Próximos marcos

1. Validar a camada unificada com controles físicos diferentes e múltiplos toques.
2. Definir uma interface JNI estável para encaminhar estados de entrada ao futuro runtime.
3. Pesquisar, de forma independente, formatos e requisitos técnicos de importação de recursos.
4. Medir desempenho, memória e compatibilidade no dispositivo Android de teste.


## Launcher: seleção local de NSP/XCI (etapa inicial)

O launcher Android permite selecionar uma cópia local de jogo em formato `.nsp` ou `.xci` pelo seletor de documentos do sistema. Ele guarda a permissão persistente de leitura do URI, registra o nome do arquivo e verifica se o acesso continua disponível ao abrir o aplicativo. O arquivo não é copiado para o armazenamento do app nem enviado pela rede.

**Limitação atual:** esta é somente a primeira camada do launcher. O projeto ainda não interpreta contêineres NSP/XCI, não descriptografa NCA, não importa chaves, não instala conteúdo e não executa o jogo. A mensagem de estado do aplicativo deixa essa limitação explícita. Utilize apenas dumps obtidos legalmente pelo próprio usuário; não compartilhe jogos ou chaves no repositório.
