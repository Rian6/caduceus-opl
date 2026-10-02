# Integração com o servidor

Esta pasta contém as alterações complementares para o
[Caduceus](https://github.com/Rian6/caduceus), outro projeto de Rian6
mantido em seu próprio repositório. O OPL não compila estes arquivos TypeScript.

- `caduceus-ra-bridge.ts`: consulta UDP de compatibilidade e sessão RA.
- `caduceus-achievements.ts`: biblioteca e progresso paginados, com pareamento por chave e sem transmitir credenciais RA.
- `caduceus-ra-art.ts`: preparação dos ícones RA para o PS2.
- `caduceus-cover-art.ts`: conversão e migração das capas para PNG.
- `integrate-*.cjs`: scripts Node.js que aplicam essas alterações no servidor
  indicado no primeiro argumento. Alteram arquivos do projeto de destino;
  use uma cópia compatível e revise o diff antes de compilar o servidor.
- `prepare-caduceus-art.cjs`: preparação da imagem de abertura e migração de
  capas, executada com Electron; altera `opl/gfx/logo.png` e a pasta ART informada.
- `refresh-installed-covers.cjs`: atualização das capas dos jogos instalados,
  também executada com Electron.

Os scripts de aplicação correspondem a etapas da integração e não são
executados automaticamente pelo build do OPL. O servidor deste ambiente
já contém essas alterações; reorganizar estas pastas não exige reaplicá-las.

Para acrescentar a aba de conquistas a um servidor que já possui as demais
integrações, execute `node integrations/caduceus/integrate-achievements.cjs /caminho/do/clone/caduceus`
e compile o Caduceus. A versão atual cria `ART/CADUCEUS.KEY` no compartilhamento
PS2 e exige essa chave nas consultas UDP 18198. Ela concede acesso de leitura
ao progresso da conta conectada; limite o compartilhamento aos dispositivos
autorizados. Não é uma senha nem uma Web API Key do RetroAchievements.
Para revogar o pareamento, encerre o Caduceus, remova a chave e reinicie o servidor.

Testes correspondentes: `tests/integration/`. Especificação do protocolo:
`docs/protocol/PROTOCOL.md`. Resultados locais: `pcsx2-test/`.
