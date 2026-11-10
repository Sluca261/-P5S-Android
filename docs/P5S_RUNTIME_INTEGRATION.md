# Integração do runtime compartilhado — Persona 5 Strikers

## Escopo desta etapa

O repositório P5S mantém o adaptador em `games/p5s/game.cpp`. O workflow `.github/workflows/runtime-integration.yml` busca o runtime de referência em um commit fixo e compila o alvo Android ARM64 com `NX_GAME=p5s` e `NX_NCE=ON`.

A etapa valida a compatibilidade de compilação entre o contrato `nx/game.h` e o adaptador P5S. Ela **não** produz um APK instalável nem demonstra que o jogo inicia.

## Origem e licenciamento

Runtime de referência: [Brandon04Gonzalez/p5r-android](https://github.com/Brandon04Gonzalez/p5r-android), commit `0c3b233d07afef37ab5af5dae41bf1a71ea98acf`. O projeto declara licença MIT. A integração mantém o runtime como checkout separado no CI, sem copiar silenciosamente código ou ativos do Persona 5 Royal para este repositório. Qualquer distribuição futura deverá preservar os avisos de licença aplicáveis.

## Limites técnicos conhecidos

- `NX_NCE` executa o código ARM64 do ExeFS do dump local; não significa que todos os serviços do Switch estejam implementados para P5S.
- O leitor de NSP/XCI e o carregamento de imagem do runtime são componentes genéricos, mas o fluxo Android de importação do P5R e seu extrator/tradutor de shaders são específicos do P5R.
- O adaptador P5S não contém offsets, hooks ou identificadores de shader do P5R.
- A integração ainda precisa de um fluxo Android seguro para selecionar o dump e as chaves localmente, verificar Title ID e Build ID, e fornecer os descritores de arquivo ao runtime sem registrar ou transmitir o conteúdo das chaves.
- É necessário identificar o backend gráfico e os requisitos específicos de P5S antes de reutilizar qualquer pipeline de shader. Não se presume que os shaders GFD do P5R sejam compatíveis.
- Áudio, vídeo, controles, salvamento, serviços de sistema e estabilidade precisam de testes reais com a cópia local do usuário.

## Critério de sucesso

O workflow desta etapa só comprova que a biblioteca ARM64 do runtime e o adaptador P5S compilam juntos. O marco de jogabilidade exige, separadamente: importação local validada, inicialização do executável, chegada ao menu, entrada em gameplay, controles e áudio funcionais, e sessões estáveis no dispositivo-alvo.
