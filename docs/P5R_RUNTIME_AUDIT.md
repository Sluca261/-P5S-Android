# Auditoria de integração do runtime de referência P5R

Data: 2026-11-09.

## Resultado principal

O projeto público P5R é uma referência tecnicamente útil e sua licença MIT permite estudar e reutilizar o código nos termos da licença. Porém, ele **não é um runtime genérico que possa ser integrado ao P5S apenas alterando o nome do jogo**. A auditoria encontrou pontos específicos de P5R que precisam ser substituídos ou reimplementados antes de uma primeira execução de Persona 5 Strikers.

## Evidências examinadas

Repositório de referência: https://github.com/Brandon04Gonzalez/p5r-android

- `CMakeLists.txt`: seleciona `games/<NX_GAME>/game.cpp`, exige CMake 3.25 e C++20 e separa código comum em `runtime/` de integração específica em `games/p5r/`.
- `games/p5r/game.cpp`: declara nome, Title ID, identificador do pacote e Build ID de Persona 5 Royal. Também contém hooks, endereços/offsets e ajustes de temporização identificados a partir do executável de P5R. Esses hooks não podem ser reaproveitados como se fossem válidos para P5S.
- `games/p5r/import/p5r_import_jni.cpp`: valida Title IDs e Build IDs de P5R e extrai os shaders de P5R. A rotina de importação precisa de uma implementação própria para Strikers.
- `README.md`: descreve o runtime como derivado do projeto Super Mario Bros. Wonder Recomp / SwitchGamesRecomp; o build nativo NCE executa o código ARM64 do jogo, enquanto a variante recompilada depende de código gerado privado.
- `games/p5r/AndroidManifest.xml`: declara a NativeActivity e bibliotecas nativas específicas do app P5R; não é um manifesto que possa ser copiado sem adaptação.
- `LICENSE`: MIT. A reutilização deve preservar avisos e atribuições e também respeitar as licenças das dependências de terceiros.

No projeto P5S atual:

- `app/src/main/cpp/CMakeLists.txt` compila somente `native-lib.cpp`, que implementa o renderizador Vulkan experimental.
- `app/build.gradle` usa C++17 e CMake 3.22.1, abaixo dos requisitos declarados pelo projeto P5R de referência.
- A seleção de NSP/XCI ainda não está conectada ao leitor de contêineres, descriptografia, carregador NCE ou runtime de serviços.

## Consequência técnica

Não é correto copiar `games/p5r/game.cpp` ou `p5r_import_jni.cpp` e apenas trocar Title ID e nome. Isso deixaria hooks, offsets, validações de versão e extração de shaders incorretos. Também não é correto afirmar que todo o diretório `runtime/` é automaticamente independente do jogo sem compilar e testar suas dependências.

A arquitetura de destino deve separar:

1. **Runtime compartilhado:** loader ARM64/NCE, kernel/IPC, serviços HLE, filesystem, NVN/Vulkan, áudio, vídeo e entrada, após auditoria de dependências.
2. **Adaptador P5S:** identificação do título e versão, requisitos de serviços, inicialização, particularidades do executável e perfil de shaders.
3. **Importador P5S:** leitura local do dump e das chaves do usuário, extração/validação dos dados e preparação de shaders, sem transmitir arquivos ou chaves.
4. **Camada Android:** launcher, permissões, armazenamento privado, NativeActivity, controles e logs.

## Próximo marco de implementação

Antes de incorporar o runtime ao build Android P5S, é necessário identificar no dump local do usuário, sem receber o dump ou as chaves, o Title ID, versão, Build ID, estrutura ExeFS/RomFS e os módulos executáveis. Esses metadados determinam se o caminho NCE é aplicável e permitem desenhar o adaptador correto.

Depois disso, o primeiro incremento de código deve ser uma camada de importação P5S isolada e testável, seguida da integração incremental do runtime compartilhado com dependências e licenças fixadas. O primeiro APK que inclua runtime só será anunciado como executável quando conseguir carregar o executável real do jogo; será chamado jogável apenas depois de chegar a uma cena controlável.

## Segurança e distribuição

O usuário deve selecionar seus próprios arquivos localmente no aparelho. NSP/XCI, `prod.keys`, `title.keys`, executáveis e assets do jogo não devem ser enviados ao GitHub, ao chat ou incorporados ao APK. Logs devem omitir material criptográfico.
