# Testes

## Host

Requer Python 3 e GCC em Linux/WSL. Não acessa contas RA nem inicia jogos.

```sh
python3 tests/run-host.py
```

Os testes extraem funções dos fontes do OPL e exercitam comandos do card,
ativação de categorias, rede, reconexão SMB e limites de texto.

## Integração

`integration/test-caduceus-bridge.cjs` requer Node.js e o Caduceus compilado:

```sh
node tests/integration/test-caduceus-bridge.cjs /caminho/do/clone/caduceus
```

O teste usa a porta UDP local 18199, separada do servidor normal.

`integration/test-achievements.cjs` usa a porta 18200 com conta e chave
fictícias. Verifica rejeição sem autorização, paginação entre blocos da API,
filtros, limite de datagramas e falhas de rede/imagens:

```sh
node tests/integration/test-achievements.cjs /caminho/do/clone/caduceus
```

O teste host `test-achievements.py` valida o parser com AddressSanitizer e
UndefinedBehaviorSanitizer, incluindo pacotes truncados e caminhos de ícone inválidos.

`integration/test-ra-art.cjs` requer o Electron do Caduceus, o caminho do
servidor e a pasta ART como argumentos. Além das fixtures temporárias,
ele consulta o catálogo local e pode gravar o ícone RA de Bully na pasta
ART informada. Não faz parte da suíte host. Relatórios ficam em `pcsx2-test/`.

As verificações visuais ficam em `tools/pcsx2/`; BIOS, perfil, saves e
capturas permanecem em `pcsx2-test/`, fora do controle de versão.
