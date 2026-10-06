# Versão de teste com card permanente

Compilar com `bash tools/build/build-xmb.sh --static-card`.
O argumento passa `RA_STATIC_CARD_TEST=1` ao loader e ao ee_core; o build força
os objetos das duas partes para evitar mistura com flags da versão de recuperação.
Sem argumento, a configuração padrão continua com o card desativado.

Nesta variante, o card usa o texto fixo:

```text
CONQUISTA DESBLOQUEADA
Get Off, You Psycho!
+1 PONTO
```

O card é iniciado quando o workspace do overlay é configurado, sem depender de
RAU1, do servidor, de autenticação ou de uma lista de conquistas. A reserva também
é feita quando a lista está vazia. Os limites de memória continuam sendo respeitados;
se não houver espaço, o card não é habilitado. O desenho começa quando o tracker
possui uma saída válida e o caminho GIF está livre. Não existe prazo de expiração.
Eventos de desbloqueio não mudam o texto de teste; os eventos de reset continuam ativos.

É estático na posição e no conteúdo, mas precisa ser reenviado quando o jogo
redesenha seu framebuffer. Pode transmitir até cerca de 4,57 MB/s a 60 Hz em
CT32, continuamente. Usa o tracker/renderizador experimental que ainda não foi
validado no PS2 físico e que teve relatos anteriores de tela preta. Não é uma
versão de produção com estabilidade ou impacto de FPS comprovados.

Artefatos separados:

- `artifacts/OPL-RA-STATIC-CARD.ELF`: esta variante de teste.
- `artifacts/OPL-RA-RECOVERY.ELF`: versão de recuperação anterior, sem card.

O vendor padrão do Caduceus continua com a versão de recuperação. Para testar,
carregar diretamente o ELF estático no PS2 e iniciar o jogo. Nenhum reinício do
servidor é necessário para trocar somente o ELF do console.

Validação: o teste host do C real cobre início sem desbloqueio, permanência
além de 10.000 frames, texto fixo mesmo recebendo outro desbloqueio, saída ainda
indisponível e reset. O teste de memória cobre reserva sem lista de conquistas,
limites de região e preservação da configuração padrão sem overlay.
## Atualização do carregamento SMB

A versão sem card com os ajustes mais recentes de startup/IOP está em
`artifacts/OPL-RA-SMB-LOAD-FIX.ELF` e no botão Salvar OPL-RA do Caduceus.
Ela evita sockets de descoberta durante SMB, adia toda descoberta/telemetria IOP
por 30 segundos, reserva o snapshot conforme o tamanho da lista do jogo e
continua o carregamento quando o módulo RA opcional falha por falta de memória.
Os testes C e o build passaram; a carga no PS2 físico ainda requer confirmação.
As variantes anteriores não incluem esses ajustes.
## Variante compacta baseada no loader validado pelo usuário

O usuário confirmou que `OPL-RA-SMB-LOAD-FIX.ELF` carregou Bully. A variante
`artifacts/OPL-RA-STATIC-LITE.ELF` mantém esses ajustes de startup SMB e IOP.
O card estático foi reduzido de 288 × 66 para 192 × 40 pixels, preservando
fundo escuro, faixa verde, troféu, título e pontos. A geometria está no header
compartilhado para o loader reservar exatamente o que o renderer utiliza.

Custos do card compacto:

- Reserva alinhada: 30.848 bytes, antes 76.160 (redução de aproximadamente 59,5%).
- GIF IMAGE CT32/CT24: 30.816 bytes por frame, antes 76.128.
- GIF IMAGE CT16/CT16S: 15.456 bytes por frame, antes 38.112.
- A 60 Hz, até 1,85 MB/s em CT32 ou 0,93 MB/s em CT16, se todos os frames
  forem desenhados. São limites calculados de transferência, não medições de FPS.
- Pixels são calculados uma vez por configuração/alteração de formato.
- Conteúdo fixo dispensa conversão de pontos/ID e truncamento de título durante
  a rasterização. Framebuffer/posição estáveis reutilizam o pacote sem operação
  de cache; troca de framebuffer/posição sincroniza só 128 bytes de cabeçalho.
- Sem espera por GIF ocupado, sem alocação durante o jogo e sem VRAM extra.

O card precisa ser transferido novamente porque o jogo redesenha a tela. Ele
não é enviado apenas uma vez, o que faria o desenho desaparecer. Ainda usa
rastreamento GS experimental; estabilidade e performance com card no PS2 físico
não foram confirmadas. O loader sem overlay validado pelo usuário foi preservado.

Build: `bash tools/build/build-xmb.sh --static-card`.
Prévia dos pixels reais do C: `python3 tests/host/test-ra-overlay.py --preview-static`
e `python tools/pcsx2/render-unlock-card.py --static`.