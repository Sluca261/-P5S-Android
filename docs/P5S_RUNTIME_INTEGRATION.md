# Integração do runtime — Persona 5 Strikers

## Arquitetura em implementação

O repositório P5S mantém um adaptador independente em `games/p5s/game.cpp` e compila o runtime compartilhado para Android ARM64 com `NX_GAME=p5s` e `NX_NCE=ON`.

O workflow `.github/workflows/runtime-integration.yml` agora prepara três componentes:
- `libp5s.so`: runtime Android/NativeActivity e adaptador P5S;
- `libspirvharness.so`: tradutor Maxwell para SPIR-V compilado para ARM64 com NativeAOT;
- APK Android que empacota as bibliotecas nativas.

A tela inicial permite selecionar o NSP/XCI e `prod.keys` localmente. O descritor do jogo é entregue ao runtime sem copiar o arquivo inteiro; as chaves são mantidas no armazenamento privado do aplicativo e não devem ser registradas em logs nem transmitidas.

## Tradução de shaders sob demanda

O runtime compartilhado normalmente procura shaders SPIR-V pré-traduzidos em `spirv/<hash>.spv`. O patch mantido em `scripts/patch_p5s_runtime.py` adiciona um fallback Android genérico: quando um shader não está no cache, o runtime entrega o bloco Maxwell e sua seção de controle ao tradutor NativeAOT, grava o SPIR-V gerado no cache local e mantém os metadados de recursos retornados pelo tradutor.

Essa estratégia evita depender do extrator de CPK/GFD específico de Persona 5 Royal. Ela ainda depende de uma premissa que precisa ser confirmada com a execução real de P5S: os blocos de shader e a seção de controle recebidos pelo backend NVN devem seguir o formato esperado pelo tradutor Maxwell compartilhado. O cache é gerado no aparelho a partir do dump do próprio usuário; nenhum shader do jogo é incluído no APK.

## Origem e licenciamento

Runtime de referência: [Brandon04Gonzalez/p5r-android](https://github.com/Brandon04Gonzalez/p5r-android), commit `0c3b233d07afef37ab5af5dae41bf1a71ea98acf`. O projeto declara licença MIT. O workflow mantém o runtime como checkout separado no CI. A integração deve preservar os avisos MIT e os avisos das dependências, inclusive Ryubing/Ryujinx.

## Limites técnicos conhecidos

- A compilação do runtime e do adaptador não prova que o jogo inicia.
- O leitor NSP/XCI e o carregamento da imagem são genéricos; serviços do Switch ainda podem exigir implementação ou adaptação específica para P5S.
- O adaptador P5S não deve receber offsets, hooks, patches de executável ou identificadores de shader de Persona 5 Royal.
- O Title ID esperado para a edição ocidental é usado apenas como referência. O arquivo local precisa ser identificado pelo runtime antes de qualquer patch específico.
- A tradução sob demanda pode causar pausas na primeira utilização de cada shader. O cache local deve reduzir esse custo em execuções seguintes.
- Áudio, vídeo, controles, salvamento, serviços de sistema, gráficos e estabilidade ainda exigem validação com P5S no dispositivo-alvo.

## Critério de sucesso para uma versão jogável

1. APK instala e abre o runtime ARM64.
2. O runtime identifica e abre o dump local, sem expor as chaves.
3. O jogo passa pela inicialização e chega ao menu.
4. Uma sessão de gameplay renderiza corretamente, com áudio e controles funcionais.
5. Salvamento/carregamento e transições de cena funcionam.
6. Sessões prolongadas não apresentam crashes ou falhas críticas.

Até que esses critérios sejam cumpridos, qualquer APK produzido pelo CI deve ser tratado como build de desenvolvimento, não como versão jogável comprovada.
