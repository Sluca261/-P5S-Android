# P5S Android

Projeto experimental de pesquisa para avaliar a viabilidade técnica de um port nativo de Persona 5 Strikers para Android.

## Estado atual

O aplicativo possui uma base Android em Java/C++, inicialização Vulkan, apresentação contínua em uma superfície Android, pipeline gráfica 3D, buffer de profundidade, iluminação direcional, textura de teste e carregamento de shaders SPIR-V empacotados no APK.

A etapa atual amplia o carregador Wavefront OBJ para aceitar bibliotecas MTL simples com cores difusas `Kd`, grupos `usemtl` e coordenadas UV. As cores dos materiais são aplicadas por vértice e as UVs alimentam a textura de teste já carregada na GPU. O parser também aceita posições, normais, faces triangulares ou poligonais e índices positivos/negativos; triangula polígonos, calcula normais ausentes, centraliza e normaliza a escala do modelo. A leitura dos arquivos é limitada e validada. O modelo e os materiais de diagnóstico são originais e estão em `app/src/main/assets/diagnostic_model.obj` e `app/src/main/assets/diagnostic_materials.mtl`.

**Isto ainda não é um port jogável de Persona 5 Strikers.** A geometria, textura e shaders presentes são recursos de teste; não há implementação do formato de dados, motor ou conteúdo proprietário do jogo.

## Limites e uso responsável

- O repositório não inclui assets, executáveis, músicas ou código proprietário do jogo.
- O teste Vulkan e a renderização 3D não comprovam que o jogo completo possa ser executado no aparelho.
- Qualquer futura análise ou importação de arquivos deve utilizar cópias obtidas legalmente pelo próprio usuário e respeitar os direitos autorais e as licenças aplicáveis.

## Próximos marcos

1. Consolidar uma camada de leitura e validação de recursos.
2. Implementar carregamento OBJ externo de malhas de diagnóstico. (concluído)
3. Interpretar materiais MTL básicos e coordenadas UV para malhas de diagnóstico. (concluído)
4. Implementar texturas independentes por material e avaliar formatos adicionais sem incorporar conteúdo proprietário.
5. Medir desempenho, memória e compatibilidade no dispositivo Android de teste.
