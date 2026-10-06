# Interface XMB do OPL

O fork usa uma composição inspirada no XMB: categorias horizontais, seleção
na coluna vertical, textos claros e fundo com gradiente e ondas animadas.
O desenho usa as coordenadas virtuais 640×480 do OPL; o renderman continua
responsável pela conversão para os modos de vídeo do console.

As categorias e listas deslizam com interpolação. A troca entre jogos e
configurações mantém o fundo contínuo, sem fade preto. Os ícones de categorias
são monocromáticos. A seleção usa texto claro sobre painel escuro e indicador
verde, sem brilho duplicado para melhorar a leitura em TVs entrelaçadas.
O fundo usa faixas opacas desenhadas diretamente pelo GS, respeitando a cor
configurada sem depender de atualização de textura dinâmica. Formulários e
teclado desenham o destaque antes do texto, sem cobrir os rótulos da linha.

## Card RetroAchievements e cores Caduceus

A paleta segue `src/styles.css` do Caduceus:
fundo adaptado para TV `#25332b`, painel `#191c18`, borda `#2a2e27`, texto secundário `#8b9284`
e destaque `#d4ef8a`. Os antigos fundos azul e quase preto padrão são migrados ao carregar;
cores de fundo personalizadas continuam respeitadas.

- **Confirmar ou Triângulo** sobre um jogo: abre o card com capa ART, nome, ID e arquivo.
- A primeira abertura de uma ISO inicia a consulta automaticamente.
- **Select** dentro do card: repete a consulta ao catálogo do Caduceus.
- O card distingue não verificado, verificando, compatível, não reconhecido,
  erro de rede/leitura e formato não suportado. Resultados e contagens vêm
  da resposta real do servidor; uma lista RA salva não é apresentada como
  uma nova confirmação de compatibilidade.
- O resultado em memória é associado ao dispositivo, nome, extensão, ID e
  formato. Não é reutilizado em outra imagem. A consulta não bloqueia o
  desenho da interface; iniciar jogos aguarda a consulta terminar.
- **Confirmar dentro do card**: jogar; **Voltar**: biblioteca; **Quadrado**: opções do jogo,
  como na biblioteca. Durante a consulta, jogar e abrir opções exibem espera
  para não interromper o acesso ao dispositivo. Não há uma segunda tela de informações.
- **L1** mostra/oculta o hash da imagem.

A consulta prepara a conexão quando o link/IP não está pronto. Se a descoberta
por broadcast não responder, tenta também o IP do servidor SMB configurado.
Isso não exige executar o teste de rede antes; o Caduceus atualizado precisa
estar aberto. O resultado é mantido ao fechar e reabrir o mesmo card.

A ponte do Caduceus usa UDP 18197: `CADQ1 <hash>` e
`CADR1 <hash> OK <quantidade> <titulo>`, `NO` ou `UNKNOWN`.
O catálogo vem de `RACompatibility`, o mesmo verificador do card no PC.
As respostas são preenchidas com NUL até múltiplos de 64 bytes (mínimo 128)
 para o transporte RPC do PS2. A consulta não baixa listas de telemetria nem
ativa conquistas durante a partida. O protocolo legado permanece separado.
Fontes da integração complementar: `integrations/caduceus/caduceus-ra-bridge.ts` e
`integrations/caduceus/integrate-caduceus.cjs`; destinados ao projeto
[Caduceus](https://github.com/Rian6/caduceus), mantido em repositório separado.

O verificador atual atende imagens em USB/BDM e ETH. O card de HDD continua
mostrando a identidade e a capa, com indicação de que a consulta de ISO não
está disponível nesse formato. Aplicativos mantêm sua tela de informações.

Teste host do estado e da associação dos resultados: `python3 tests/host/test-ra-card.py`.
Teste da preparação de rede e limpeza de sockets: `python3 tests/host/test-ra-network.py`.
Teste de limites do texto com AddressSanitizer: `python3 tests/host/test-font-wrap.py`.

## Capas e abertura Caduceus

Este fork carrega `ART/<Game ID>_COV.png`. O Caduceus agora exporta PNG
192×272 para novas capas e converte capas JPG antigas na inicialização e ao listar/reparar
instalados, sem apagar os originais. A presença de um JPG não é mais suficiente
para marcar a capa como instalada para este OPL. Imagens inválidas não
substituem uma capa válida. Os ajustes estão em `electron/cover-art.ts` no
projeto complementar, reproduzíveis por `integrations/caduceus/integrate-caduceus-covers.cjs`.

A imagem do catálogo RA é exportada como `ART/<hash>_RA.png`, 64×64, antes
da resposta de compatibilidade. O card usa um cache separado de uma textura
(16 KiB de pixels RGBA), sem criar uma textura de tela inteira. Falha de rede
ou imagem ausente não altera o resultado da compatibilidade. Em USB/BDM, é
necessário copiar também esse arquivo ART do servidor para o dispositivo.
Integração: `integrations/caduceus/integrate-caduceus-ra-art.cjs`; testes Electron:
`tests/integration/test-ra-art.cjs`. A sombra do card é desenhada antes dos painéis; o
fundo tem preenchimento base e faixas sobrepostas para evitar emendas pretas
por arredondamento de coordenadas entre modos PAL/NTSC.

`opl/gfx/logo.png` reutiliza `public/ps2-splash.png` do Caduceus, reduzida
para 512×288; a abertura mantém a proporção da arte. A preparação e os testes
de conversão usam `integrations/caduceus/prepare-caduceus-art.cjs` com o Electron do Caduceus.

## Rede e compartilhamento

O padrão de inicialização do ETH é **Auto** e o compartilhamento padrão é
**PS2**. Configurações antigas que ainda usam exatamente `ps2` são migradas
para `PS2` ao carregar. Outros nomes de compartilhamento e opções explicitamente
salvas continuam respeitados.

SELECT/Refresh agora verifica a sessão SMB no worker de I/O e reconecta quando
há erro, antes de forçar uma nova leitura da biblioteca. Uma sessão saudável
apenas relê a lista, mesmo que os timestamps não tenham mudado. O driver libera
a conexão antiga inclusive quando o servidor não responde ao logoff ou quando
a autenticação falha. Adaptador de rede ausente continua sendo um erro:
no PCSX2, a emulação de Ethernet/DEV9 precisa estar habilitada para testar SMB.
O fechamento do socket também deixou de esperar pelo FIN de um servidor que
pode ter desaparecido.

Teste de regressão do ciclo da sessão, com respostas de rede simuladas:

```sh
python3 tests/host/test-smb-reconnect.py
```

Validação desta revisão: ELF compilado com PS2SDK e PADEMU=1; testes host de
encerramento, autenticação e transporte aprovados; navegação Games/Settings
verificada no PCSX2 com capturas nos dois sentidos, sem escurecimento do fundo.
A recuperação de uma biblioteca em um servidor SMB real ainda precisa ser
testada com o compartilhamento e o adaptador de rede disponíveis.

## Navegação

USB/BDM, HDD e aplicativos aparecem desde a inicialização em modo Manual:
selecione a categoria e confirme para iniciar o dispositivo. ETH permanece
em Auto por padrão. Perfis antigos com BDM/HDD/aplicativos desativados são
carregados como Manual para disponibilizar essas categorias no XMB;
os que já usam Auto continuam em Auto.

- Esquerda/direita: categorias de dispositivos; à esquerda da primeira fica
  Configurações. Direita em Configurações abre a primeira categoria disponível.
- START: abre o menu de sistema; START ou o botão de voltar retorna aos jogos.
- Cima/baixo: seleção de jogos ou opções. A seleção permanece visível mesmo
  nas listas longas de configurações por jogo e ações de RetroAchievements.
- Os comandos indicados no rodapé vêm do dispositivo ativo e respeitam a
  escolha entre círculo e cruz para confirmar.
- Os formulários mantêm os controles originais (IP, VMC, GSM, enums etc.) e
  rolam quando o campo selecionado ultrapassa a área disponível.

As capas, ícones, selos RA e metadados continuam vindo do sistema de temas e
do cache ART. A composição da biblioteca é fixa neste fork; posições de
temas externos não substituem a composição XMB. Na tela de informações,
os metadados e imagens dinâmicas são preservados, enquanto imagens estáticas
de fundo/painel cedem lugar ao fundo compartilhado. A cor de fundo configurada
continua ajustando o gradiente. Não são necessárias novas imagens de fundo.

## Ações do card e sessão RA

Nas listas de jogos e aplicativos, o rodapé mostra somente Atualizar
(Select). As dicas de configurações continuam disponíveis. Entrar numa
categoria inicia o dispositivo e agenda a busca depois dos módulos,
sem exigir confirmação ou atualização manual. A primeira categoria USB
permanece visível sem dispositivo; desconectar limpa os jogos antigos.
No card, esquerda
e direita alternam Jogar e Voltar; o botão de confirmação executa a ação
selecionada e o botão de cancelar sempre retorna à lista.

Jogar fica indisponível somente enquanto a consulta está pendente. Ao
terminar, jogos sem conquistas, sessão desconectada e falhas de consulta
permitem jogar sem RA. Uma sessão desconectada não carrega listas RA
antigas. O card consulta novamente ao abrir e informa a sessão recebida
do Caduceus via CADQ2/CADR2. A compatibilidade do catálogo não garante
que exista uma lista de telemetria instalada para aquele jogo.

Testes: `tests/host/test-card-actions.py` (comandos e liberação do lançamento),
`tests/host/test-ra-card.py` (identidade e publicação do worker) e
`tests/integration/test-caduceus-bridge.cjs` (UDP com e sem sessão, versões 1/2).

## Biblioteca de conquistas

A categoria Conquistas fica após Aplicativos no XMB e contém a opção
Visualizar conquistas. Confirmar essa opção abre os jogos da conta conectada
ao Caduceus; cancelar na biblioteca retorna ao submenu. Apenas navegar até
a categoria não inicia uma consulta. O card de uma
ISO compatível também oferece acesso direto pelo hash, mesmo que o jogo
ainda não apareça no histórico da conta.

Cada página contém três entradas, com ícones de 64×64, título, pontos e
estado. A descrição e a data correspondem à entrada selecionada. L1/R1
mudam a página; quadrado alterna Todas, Desbloqueadas, Pendentes e Hardcore;
Select atualiza. Cancelar retorna mesmo durante a consulta. Os filtros
não mudam o modo de execução do jogo.

O servidor precisa da integração `caduceus-achievements.ts`. A chave de
pareamento fica em `ART/CADUCEUS.KEY`, no compartilhamento privado PS2;
o OPL a lê por ETH. Credenciais RetroAchievements permanecem no Caduceus.
Uma sessão ausente ou erro exibe uma mensagem sem impedir o retorno ao OPL.

A consulta é feita no worker de I/O. O console mantém apenas uma página e
um cache de três imagens. Enquanto a consulta termina, o lançamento e a
reconexão aguardam para não encerrar os recursos usados pelo worker.

## Compilação

No ambiente WSL deste projeto:

```powershell
wsl -d Ubuntu-24.04 -- bash /mnt/d/xerabora-caduceus-xmb/tools/build/build-xmb.sh
```

O script usa `PS2DEV` quando definido, ou `/usr/local/ps2dev` como padrão
local, e compila com `PADEMU=1`. Ele força a recompilação dos fontes da
interface para evitar reutilizar objetos de uma compilação com outros flags.

Saída executável: `opl/OPNPS2LD.ELF`. Log: `opl/xmb-build.log`.

## Validação no console ou emulador

A compilação não substitui a validação visual e de desempenho no PS2.
Antes de distribuir, verificar:

- PAL/NTSC e proporções 4:3/16:9, incluindo overscan e textos longos.
- Dispositivo vazio/desativado, atualização de lista, paginação e primeiro/último jogo.
- Capas ausentes, aplicativos e jogos com selo de RetroAchievements.
- Configurações de rede, GSM, VMC, teclado, confirmações e seleção de cheats.
- Rolagem até as últimas opções, salvar/carregar e alternar círculo/cruz.
- Fluidez das ondas e uso de memória com listas grandes no hardware real.
