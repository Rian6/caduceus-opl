# Card de conquistas integrado

Versão conectada publicada em `artifacts/OPL-RA-ACHIEVEMENTS.ELF` e no vendor
`<CADUCEUS_DIR>\vendor\xerabora\OPL-RA.ELF`, usado por Salvar OPL-RA.
SHA-256: `ef944496b3b28ea08ccd60f331e8ebf23889c62b0226d94a6f2912c6daa673a7`.
Build: `bash tools/build/build-xmb.sh --achievements`.

O usuário confirmou carregamento normal e card estático compacto visível em
Bully no PS2 físico por SMB. Esta versão liga o mesmo layout aos desbloqueios
reais, sem card de demonstração ao iniciar. O Caduceus já chama
`notifyCaduceusUnlock(value)` no evento xera:unlock. O datagrama RAU1 contém ID,
pontos e título; a caixa SMAP, o parser raudp e o DMA confirmado entregam o evento
ao EE. O card aparece no canto superior direito por 210 VBlanks (3,5 s em 60 Hz;
4,2 s em 50 Hz), e novos eventos substituem o card ativo. RAU1 antigo sem título
mostra o ID. Títulos têm até 19 caracteres visíveis; mais longos usam reticências.

Layout compartilhado de 192 × 40 pixels: fundo escuro, faixa verde, troféu,
CONQUISTA DESBLOQUEADA, título e +N PONTO/PONTOS. Reserva alinhada de 30.848 bytes.
Transferência de 30.816 bytes em CT32/CT24 e 15.456 em CT16/CT16S, somente durante
a notificação. Pixels são refeitos apenas para evento/formato novo. Framebuffer
estável não sincroniza cache; troca de destino sincroniza 128 bytes. Não há
alocação durante o jogo ou reserva de VRAM extra.

Continuam as correções validadas de carregamento: descoberta SMB sem socket,
espera inicial da telemetria, snapshot IOP proporcional e falha RA sem loop.
A configuração padrão de desenvolvimento sem argumento conserva o modo sem
card; --static-card continua gerando o teste permanente; --achievements gera
notificações conectadas. As flags são aplicadas e recompiladas no loader e core.

O usuário observou piscadas no teste estático. Elas não foram eliminadas nesta
versão. GIF ocupado continua pulando o desenho, e o jogo pode redesenhar o
framebuffer depois do overlay. A causa exata exige observação do console;
alterar o sincronismo/introduzir espera sem validação poderia afetar a partida.
Não foi alterado esse mecanismo que acabou de funcionar no hardware.

Passaram testes host do renderer C, estado oculto antes do evento, duração,
título longo/pontos, formatos, buffer/cache, reset, memória, parser/notificação
SMB, startup SMB e alocação/falha IOP. Passou o teste do bridge compilado do
Caduceus com rede simulada. O build completo passou. A cadeia de desbloqueio
real no PS2 com este novo ELF ainda precisa ser confirmada pelo usuário.

O servidor não foi reiniciado. Backups em
`pcsx2-test/caduceus-before-connected-card/`. O ELF estático compacto e a versão
sem card que carregou foram preservados em artifacts. Salvar/carregar o novo
ELF no PS2 e relançar Bully para ativar a versão integrada.
## Refinamento da preparação em interrupção

Após relato de travamento ao exibir uma conquista, o overlay passou a usar
`iSyncDCache`, declarado no SDK para o contexto de interrupção, em vez de
`SyncDCache` dentro do handler VBlank.

A rasterização agora prepara quatro linhas por chamada: até 768 pixels e
3.072 bytes em CT32. Cada bloco é sincronizado separadamente. O cabeçalho só
é finalizado e o DMA GIF só é iniciado depois das 40 linhas prontas. Isso
leva dez frames livres (cerca de 167 ms em 60 Hz). O último passo sincroniza
apenas 128 bytes de cabeçalho; não há flush do pacote inteiro no desbloqueio.
Textos fora das linhas em preparação retornam sem percorrer os caracteres.

Novos eventos reiniciam a preparação. Troca entre 16/32 bits também reinicia
e mantém o estado pendente até completar o pacote. GIF ocupado não modifica
nem avança o buffer. Reset continua sendo processado imediatamente. A troca
do workspace limpa o estado de preparação para não reutilizar dados antigos.

A transferência do card pronto continua com 30.816 bytes em CT32/CT24 ou
15.456 em CT16/CT16S. O refinamento reduz o pico de trabalho da interrupção,
sem representar uma medição de FPS ou confirmação de correção no console.