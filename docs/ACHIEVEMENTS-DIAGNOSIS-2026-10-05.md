# Diagnóstico local das conquistas — 2026-10-05

## Causa observada

O processo ativo usa `C:\Program Files\Caduceus\Caduceus.exe`.
O `resources/app.asar` instalado informa versão 1.1.0. Seu entrypoint
`dist-electron/main.js` não inicializa `startCaduceusRABridge` nem
`startCaduceusAchievements`; o pacote não inclui esses módulos.
O processo auxiliar Xerabora escuta UDP 18194,
mas não há listeners em UDP 18197 e 18198 neste computador.

O card do OPL consulta compatibilidade e sessão por UDP 18197;
a biblioteca e a lista acessada pelo card usam UDP 18198 e uma
chave de pareamento no compartilhamento ETH. Portanto, a instalação
em execução não oferece os serviços esperados por esta versão do OPL.
Isso explica as consultas sem resposta no ambiente inspecionado.

O clone em `<CADUCEUS_DIR>` informa versão 1.2.0 e já inclui
`startCaduceusRABridge`, `startCaduceusAchievements` e criação da chave
em `electron/main.ts`, além dos módulos compilados correspondentes.
É necessário compilar/empacotar e executar uma instalação contendo essas
integrações. Apenas reiniciar a versão instalada ou alterar o firewall
não acrescenta os protocolos ausentes. Nenhuma instalação ou processo
ativo foi substituído nesta investigação.

## Validação

- As nove suítes host existentes passaram no WSL: parser, navegação,
  comandos do card, sessão, rede, categorias, texto e reconexão SMB.
- Os dois testes de integração passaram contra os módulos compilados
  do clone local, com contas fictícias e portas locais 18199/18200.
  Cobrem compatibilidade, sessão conectada/desconectada, abertura por
  hash, autenticação de pareamento, filtros e paginação.
- O build `tools/build/build-xmb.sh` passou e gerou
  `opl/OPNPS2LD.ELF`. Ele inclui as alterações preexistentes no workspace.
- Corrigidas constantes multicaractere `\\r`, `\\n`, `\\t` em
  `opl/src/achievements.c`, substituindo-as por escapes de caracteres.
  O novo teste host exercita o leitor da chave e compila com `-Werror`.
  Esta correção é secundária: a leitura limitada a 64 bytes já ignora
  uma quebra de linha após uma chave completa válida.

## Limites e conferência após atualização

Os testes executam funções reais do console em harnesses host e o
protocolo do servidor via UDP. Não houve teste visual novo no PCSX2,
teste em PS2 físico nem desbloqueio real durante uma partida.

Após executar a instalação integrada, conferir listeners UDP 18197 e
18198, compartilhamento ETH ativo e `ART/CADUCEUS.KEY` disponível.
No console, abrir um jogo compatível, conferir o estado da sessão,
selecionar Conquistas, testar filtros/páginas e retornar ao card.
Desbloqueios durante o jogo dependem também do Xerabora e da telemetria;
o sucesso da consulta do card não comprova esse fluxo.

## Validação posterior no PCSX2 — concluída

A pedido do usuário, servidor integrado e PCSX2 foram iniciados.
A inspeção final confirma que o pacote local 1.2.0 também inclui os
dois módulos e suas inicializações. A ausência das strings dos protocolos
no entrypoint, isoladamente, não prova ausência da integração: elas ficam
nos módulos importados. A instalação ativa original 1.1.0 realmente não
os inclui.

O servidor ficou em execução via Electron do clone, usando
`tools/pcsx2/start-caduceus-local.cjs`. Esse launcher carrega
`<CADUCEUS_DIR>\dist-electron\main.js` e direciona a interface para
`dist/index.html`, evitando depender do Vite. Não altera os arquivos do
clone nem a instalação em Program Files. Inicializar depois com:

```powershell
Remove-Item Env:\ELECTRON_RUN_AS_NODE -ErrorAction SilentlyContinue
Start-Process '<CADUCEUS_DIR>\node_modules\electron\dist\electron.exe' `
  -ArgumentList '<OPL_DIR>\tools\pcsx2\start-caduceus-local.cjs <CADUCEUS_DIR>' `
  -WorkingDirectory <CADUCEUS_DIR>
```

Foram observados listeners UDP 18194/18197/18198 e SMB TCP 1024.
Consultas reais retornaram a biblioteca e Bully (ID 3057), com
120 conquistas e uma desbloqueada.

No PCSX2 2.8.2, executando `opl/OPNPS2LD.ELF`, foram confirmados:

- Categoria Conquistas e abertura da biblioteca da conta.
- Bully com ícone e progresso 1/120.
- Lista de conquistas com ícones, pontos e descrições.
- Paginação de 1/40 para 2/40.
- Filtro Desbloqueadas com uma conquista e data de desbloqueio.
- Filtro Hardcore vazio, sem falha de navegação.
- Retorno da lista à biblioteca e ao submenu.
- Card da ISO Bully (USA), SLUS_212.69, indicando compatibilidade,
  120 conquistas e sessão RA conectada.
- Abertura de Conquistas diretamente pelo card e retorno ao mesmo card.

Capturas locais em `pcsx2-test/`: `live-library-latest.png`,
`live-bully-all.png`, `live-bully-page2.png`, `live-bully-earned.png`,
`live-bully-hardcore.png`, `live-game-card-check.png`,
`live-card-conquistas-ready.png` e `live-card-return.png`.

O perfil de teste usa rede Sockets, DHCP interno com IP <LOCAL_IP>,
máscara 255.255.255.0 e gateway <LOCAL_IP>. Backup anterior em
`pcsx2-test/PCSX2-before-live.ini`. Na inicialização final foi mantido
Start/Enter para ignorar a carga de configurações antigas do OPL;
a renderização foi alternada com F9 para software. Servidor e emulador
permaneceram abertos ao concluir. Não foi iniciada uma partida, portanto
desbloqueios durante jogo e PS2 físico continuam fora desta validação.

## Correção do fluxo durante o jogo — 2026-10-05

O usuário confirmou Bully em PS2 físico por SMB. Foi observada conexão SMB
ativa de <PS2_IP> ao servidor <SERVER_IP>, mas zero pacotes no Xerabora.
O servidor estava autenticado. A inspeção encontrou `sbHashGame` chamando apenas
`raAskCaduceus`: essa consulta verifica catálogo/conta, mas não obtém a lista de
memória. A pasta RA do compartilhamento não continha a lista de Bully. Sem
lista, `raWatchCount` fica zero e `iopmgr.c` não carrega o módulo de telemetria.

Correções aplicadas:

- O worker de verificação agora chama `raAskPC` para uma sessão autenticada e
  só habilita conquistas durante o jogo se recebeu uma lista válida e não vazia.
  A falha é apresentada no card, com possibilidade de tentar novamente via SELECT.
- A identificação do Xerabora tenta primeiro o PC configurado e depois broadcast.
- Integrador aplicado e Electron compilado em `<CADUCEUS_DIR>`: o evento
  `xera:unlock` chama `notifyCaduceusUnlock`, enviando título e pontos ao PS2.
- O ELF do botão “Salvar OPL-RA” também foi atualizado em
  `<CADUCEUS_DIR>\vendor\xerabora\OPL-RA.ELF`, com checksum atualizado.
- Corrigido o build: `make -W` no processo pai não forçava os objetos no make
  recursivo, permitindo mistura de flags PADEMU. Os objetos são recompilados
  diretamente e os módulos IRX ficam prontos antes de serem embutidos.

A identificação real da ISO de Bully e os três chunks responderam pelo Xerabora
ativo: 2.276 bytes, 561 endereços diretos, 697 bytes de valores diretos e 120
conquistas (uma desbloqueada). Essa lista foi validada e publicada em
`<SMB_DIR>\PS2\RA\SLUS_212.69.wl` para o próximo lançamento.
O teste não submeteu snapshots nem concedeu conquistas.

Passaram os testes host do fluxo completo de hash → catálogo → lista → prontidão,
card, comandos, rede, recepção SMB e overlay. Passaram os testes do bridge
compilado e do envio de título/pontos. O novo ELF foi compilado e seu SHA-256 é
`cb402efb9266863b7fd2f2ab128cca449b063c237a7c2f2a1c4ec632cff76b39`.
Backups do Caduceus e do ELF anterior estão em
`pcsx2-test/caduceus-before-notice-fix/`.

O processo do Caduceus não foi reiniciado enquanto a partida SMB estava ativa.
A ativação do código Electron compilado exige reinício, e o PS2 precisa carregar
o novo ELF e relançar Bully. O retorno da telemetria e um desbloqueio visual real
ainda precisam ser verificados nessa nova sessão; testes host não comprovam isso.

## Estabilidade do overlay — 2026-10-05

Após relato de travamentos em Bully no PS2 físico por SMB, a revisão encontrou
riscos concretos no desenho e no rastreamento recém-adicionados:

- O modo passivo reutilizava o breakpoint GSM de leitura/escrita, incluindo a
  página CSR/IMR. Consultas de status do jogo podiam gerar muitas exceções.
  Passou a usar somente escritas (0x20280000) e máscara 0x1FFFFF0F, excluindo
  essa página. A configuração do GSM normal não foi alterada.
- As primitivas do card modificavam registradores persistentes de desenho.
  O card agora usa GIF IMAGE, transferindo pixels para o framebuffer exibido
  sem modificar os contextos de desenho do jogo.
- O pulso amarelo foi removido. Saída indisponível ou GIF ocupado resulta em
  nova tentativa dentro da duração do card, sem espera no hook.
- Reserva fixa alinhada de 76.160 bytes fora do BSS do ee_core. A reserva do
  card não pode ultrapassar 0x00100000 na região baixa ou 0x02000000 quando
  relocada. Se não couber, continua a telemetria sem instalar o rastreador passivo.

Passaram os testes host de configuração assembly do breakpoint, limites de
memória da reserva, GIF IMAGE, formatos, caminhos ocupados, troca de buffer,
DMA incompleto e reset, além dos testes de lançamento RA, rede, recepção SMB
e comandos do card. O build completo passou; a stack do ee_core tem 7.680 bytes,
acima do limite mínimo de 3.072 bytes imposto pelo linker.

Novo ELF: `opl/OPNPS2LD.ELF`, SHA-256
`bea7946e5f2ffb9146ae75d229504415808a8519dd64a3d4b18b4dea38b7148a`.
Publicado também em `<CADUCEUS_DIR>\vendor\xerabora\OPL-RA.ELF`, com checksum
atualizado e backup em `pcsx2-test/caduceus-before-stability-fix/`.
O servidor não foi reiniciado. É necessário salvar/carregar esse novo ELF no
PS2 e relançar Bully. Estabilidade e custo real no console continuam pendentes
de validação; a simulação host não reproduz toda a concorrência GIF/VIF do jogo.
## Recuperação após tela preta no carregamento — 2026-10-05

O usuário informou que a versão anterior ainda não carregava Bully: demora e
apenas tela preta. A correção de máscara do breakpoint não foi suficiente.
A revisão confirmou que o rastreador era instalado antes de executar o jogo e
que o handler reaproveitado agrupa diferentes opcodes de escrita como `sd`.
Isso impede tratá-lo como um observador transparente de qualquer jogo.
Não foi comprovado que esse seja o único motivo da tela preta no console.

Versão de recuperação distribuída:

- `RA_ENABLE_EXPERIMENTAL_CARD=0` por padrão, em `ra_features.h` compartilhado.
- Conquistas não instalam/removem GSHandler ou Hook_SetGsCrt. O GSM solicitado
  explicitamente pela configuração do usuário conserva seu caminho normal.
- O loader não reserva os 76.160 bytes de pixels do card.
- Eventos de desbloqueio são consumidos sem iniciar DMA GIF ou sincronizar
  pixels. Eventos de reset mantêm confirmação/deduplicação e funcionamento.
- Lista de memória, snapshots, raudp e recepção SMB permanecem implementados.

Passaram testes das duas configurações do renderer e da reserva, além do teste
de pré-processamento dos caminhos reais de boot/reset. A configuração padrão
foi usada nos testes de recuperação, a partir do header compartilhado real.
Passaram também testes do lançamento RA e da recepção SMB. O build completo
passou; a inspeção das relocations MIPS de main.o/padhook.o confirmou ausência
de chamadas a EnableGSTracker/DisableGSTracker e presença dos caminhos GSM
configurado e RA. A stack do ee_core ficou em 10.560 bytes (0x2940).

Novo ELF: `opl/OPNPS2LD.ELF`, SHA-256
`6a7f314de9237e8ae7b433aa904207551295b57aa9d4bef0a79b8483716ca8e0`.
A mesma versão foi publicada no vendor do Caduceus, com checksum atualizado.
Backup da versão substituída: `pcsx2-test/caduceus-before-recovery-build/`.
O servidor não foi reiniciado. O usuário precisa salvar/carregar novamente o
ELF e relançar o jogo; recuperação do carregamento no PS2 físico ainda não
foi confirmada. O card permanece desativado nesta versão. A prévia anterior
representa o renderer experimental, não uma funcionalidade validada no console.
## Variante de teste com card permanente — 2026-10-05

Criada a pedido do usuário, com `RA_STATIC_CARD_TEST=1` no loader e no ee_core.
O workspace do overlay inicia um card fixo (Get Off, You Psycho!, +1 PONTO),
sem depender de evento de desbloqueio e sem prazo de expiração. Desbloqueios
posteriores não modificam esse texto de teste; eventos de reset são preservados.
O workspace é preparado também sem lista de conquistas, e o tracker depende
somente da existência desse buffer. O desenho aguarda configuração de saída
válida e GIF livre; mantém a proteção de limites da reserva de memória.

Build: `bash tools/build/build-xmb.sh --static-card`. O script recompila tanto
objetos do loader quanto do core ao trocar as flags. O padrão sem argumento
continua desativando o card. Foram testadas permanência, inicialização sem evento,
texto fixo, saída indisponível, reset e reserva de memória com/sem lista.
A inspeção do objeto MIPS confirmou a reserva de 76.160 bytes na variante estática.

ELF separado em `artifacts/OPL-RA-STATIC-CARD.ELF`; a versão de recuperação foi
preservada em `artifacts/OPL-RA-RECOVERY.ELF`. O vendor padrão do Caduceus não foi
substituído. A variante reativa o mecanismo experimental de rastreamento/render
que ainda precisa de validação no PS2 físico e transmite pixels continuamente
para manter o card no framebuffer redesenhado pelo jogo. Não foi demonstrada
estabilidade em Bully; essa é uma versão de teste visual solicitada pelo usuário.
Detalhes em `docs/STATIC-ACHIEVEMENT-CARD.md`.
## Correção do startup SMB/IOP — 2026-10-05

O usuário confirmou tela preta imediatamente após selecionar Bully, tanto no
ELF de recuperação quanto no de card estático. Uma inspeção somente de leitura
confirmou OPLServer em <SMB_DIR>, porta TCP 1024 e
conexão estabelecida de <PS2_IP> para <SERVER_IP>. Não houve reinício do servidor.
A conexão confirma alcance SMB, mas não prova que a primeira carga do ELF terminou.

A revisão encontrou dois problemas no caminho comum de inicialização:

- `ra_thread` chamava a descoberta via socket antes do período de silêncio de
  30 segundos. Em SMB, apesar de a recepção do loop principal estar desativada,
  a descoberta continuava abrindo/recebendo por lwIP durante o carregamento.
  Agora a espera precede toda atividade de descoberta. No modo SMB, a descoberta
  usa somente o host configurado e a tabela ARP já preenchida pelo tráfego SMB;
  não abre socket, não envia consultas RAP1 e não chama recvfrom. Host/MAC
  ausente faz a telemetria sair sem bloquear o jogo. Outros modos conservam
  descoberta via socket. Notificações SMB continuam chegando pela caixa SMAP.
- O snapshot IOP usava sempre RA_SNAP_TOTAL (maior conjunto), em vez do tamanho
  do jogo. Agora reserva RA_SNAP_TOTAL_FOR(bytes + nodes*8), com validação de
  limites. A lista de Bully usada nos testes tem 697 bytes e precisa de uma
  reserva alinhada de 768 bytes. Se faltar memória, não carrega raudp sem
  argumentos. Se o módulo falhar, limpa ra_snap_iop e libera o snapshot.
  O erro -400 do módulo opcional RAUDP não entra mais no loop infinito do
  loader; módulos essenciais conservam o tratamento anterior.

Testes do código C real: descoberta SMB sem qualquer socket, host/MAC ausentes,
espera anterior ao primeiro acesso de rede, caminho não SMB preservado,
alocação proporcional, limites de chains, falta de heap e falha -400 do módulo
opcional com retorno em vez de travamento. Passaram também testes de recepção
SMB, lançamento RA e configuração padrão sem hook gráfico.

Esses problemas foram corrigidos, mas não está comprovado que expliquem toda
a tela preta do PS2 físico. O próximo teste deve usar o ELF novo sem overlay;
os artefatos estático/recuperação anteriores não incluem essas correções.
Build completo concluído. ELF novo sem card:
`artifacts/OPL-RA-SMB-LOAD-FIX.ELF`, SHA-256
`06b550fd65711b49fe6820111f039e30da3db8b7dc197433a286b44d2e010976`.
A mesma versão foi publicada no vendor do Caduceus com checksum atualizado;
backup em `pcsx2-test/caduceus-before-smb-load-fix/`. O servidor permaneceu ativo.
O teste físico de carregamento ainda está pendente; usar esse novo artefato ou
salvar novamente pelo botão Salvar OPL-RA e carregar o ELF no PS2.
## Card fixo compacto após confirmação do carregamento — 2026-10-05

O usuário confirmou que o ELF com correções de load carregou Bully. Preparada
uma variante baseada nesse código, preservando descoberta SMB sem socket,
espera anterior à descoberta, snapshot IOP proporcional e falha RA sem loop.

Card fixo compacto: 192 × 40 pixels, com o mesmo título de teste e +1 PONTO.
Workspace alinhado de 30.848 bytes, contra 76.160 da variante estática anterior.
Transferência CT32/CT24 de 30.816 bytes por frame, contra 76.128; CT16/CT16S de
15.456 contra 38.112. Economia de aproximadamente 60% nos bytes transferidos.
Conteúdo fixo não formata título/pontos. Pacote e pixels são reaproveitados;
framebuffer/posição estáveis não executam SyncDCache. Troca de destino modifica
somente o cabeçalho e sincroniza 128 bytes. A primeira rasterização sincroniza
uma única vez o pacote completo, já com cabeçalho finalizado.

Passaram os testes do renderer C (inclusive permanência e ausência de flush em
frames estáveis), reserva de memória, startup SMB e alocação/falha IOP. Build
completo passou; o linker deixou 8.256 bytes para a stack do ee_core.
Prévia gerada dos pixels do C em pcsx2-test/ra-static-card-detail.png.

Artefato: artifacts/OPL-RA-STATIC-LITE.ELF, SHA-256
10e6cc35beafe5f37d37e2bd0d053127334509b92e98aaa8837979c89e69622c
A versão sem card que o usuário confirmou carregar permanece em
artifacts/OPL-RA-SMB-LOAD-FIX.ELF, SHA-256
06b550fd65711b49fe6820111f039e30da3db8b7dc197433a286b44d2e010976
O vendor padrão do Caduceus não foi trocado, nem o servidor reiniciado.

A estabilidade com o card no PS2 físico ainda requer validação. As reduções de
bytes são calculadas pelo pacote compilado/testado, não equivalem a uma medição
de FPS ou garantia de impacto zero. O rastreador GS continua experimental.
## Card compacto conectado às conquistas — 2026-10-05

O usuário confirmou que o card fixo apareceu no PS2 físico, com piscadas
ocasionais. Preparada a versão integrada usando o mesmo layout de 192 × 40,
com título/pontos recebidos por RAU1 e duração de 210 VBlanks. O card fica oculto
antes do desbloqueio e não usa texto fixo de demonstração. O custo permanece
30.816 bytes por frame em CT32/CT24, apenas enquanto a notificação está ativa.
Títulos longos recebem reticências; o ID é usado se o protocolo não traz título.

O piscar não foi eliminado: preservado o desenho sem espera quando GIF ocupado.
A causa exata e o sincronismo do jogo não foram medidos no hardware. Não foi
introduzida uma alteração arriscada de sincronismo na implementação validada.

Build --achievements e testes de renderer, RAU1/SMB, startup, memória e bridge
compilado passaram. Callback notifyCaduceusUnlock confirmado no main.ts/main.js
reais do Caduceus. Nenhuma conquista foi concedida artificialmente nos testes.

Artefato: artifacts/OPL-RA-ACHIEVEMENTS.ELF; mesmo conteúdo publicado no vendor
do Caduceus, com SHA-256
fe35b466e2d6712937ebd6ee6ab91666e1a45980afb31210e5cd49c497eb57b8
Backup em pcsx2-test/caduceus-before-connected-card/. Servidor não reiniciado.
A validação de um desbloqueio real com este ELF integrado ainda está pendente.
## Preparação limitada por frame e cache de interrupção — 2026-10-05

O usuário relatou travamento no momento de exibir a conquista. Foi revisado o
caminho executado dentro do handler VBlank, incluindo preparação inicial do card.
O overlay usava SyncDCache nesse contexto. O kernel.h do SDK instalado declara
iSyncDCache, e o overlay foi alterado para essa variante de interrupção.
A causa exata no PS2 não foi comprovada por trace; esse uso e o pico de trabalho
foram corrigidos sem declarar o problema físico definitivamente resolvido.

Rasterização/cache agora processam quatro linhas por chamada, até 3.072 bytes
em CT32. As 40 linhas levam dez chamadas livres; pacote parcial nunca inicia
DMA GIF. Linhas são sincronizadas individualmente e o último passo sincroniza
só 128 bytes do cabeçalho. Textos fora do bloco em preparação são descartados
antes de percorrer seus caracteres. O payload do card pronto continua igual.
Novos eventos e troca 16/32 bits reiniciam a preparação; GIF ocupado não avança
nem modifica o pacote. A troca de workspace cancela estado antigo. Reset continua
sendo tratado mesmo durante a preparação.

Passaram testes C de trabalho/cache limitados por chamada, envio somente após
conclusão, troca de formato sem novo evento, novo evento no meio da preparação,
GIF ocupado, reset, título longo/pontos, formatos e cache de frames estáveis.
Passaram parser/recepção SMB e alocação/falha IOP. Build completo passou.
Relocations do objeto MIPS ra_overlay.o confirmaram somente chamadas a
iSyncDCache (duas), sem SyncDCache. A stack do ee_core ficou em 7.168 bytes.

ELF refinado: artifacts/OPL-RA-ACHIEVEMENTS-REFINED.ELF; também atualizado o
artefato genérico de conquistas e o vendor usado pelo botão Salvar OPL-RA.
SHA-256: ef944496b3b28ea08ccd60f331e8ebf23889c62b0226d94a6f2912c6daa673a7
Backup em pcsx2-test/caduceus-before-interrupt-refinement/. Servidor não reiniciado.
O teste físico de desbloqueio com esta versão ainda está pendente.