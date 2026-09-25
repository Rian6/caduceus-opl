/* ---- LIBRARY: every game the account touched, from the Web API --------- */

/* HighestAwardKind: mastered and completed are the hardcore and softcore
   full clears; beaten comes in the same two flavours. */
function awardBadge(g) {
  const a = g.award || '';
  if (a === 'mastered') return html`<span class="award gold">${t('MASTERED')}</span>`;
  if (a === 'completed') return html`<span class="award green">${t('COMPLETED · SC')}</span>`;
  if (a === 'beaten-hardcore') return html`<span class="award blue">${t('BEATEN')}</span>`;
  if (a === 'beaten-softcore') return html`<span class="award blue">${t('BEATEN · SC')}</span>`;
  return null;
}

function GameCard({ g }) {
  const art = g.icon ? MEDIA + g.icon : '';
  return html`<div class="card" onClick=${() => openGame(g.id)}>
    <div class="art">
      ${art && html`<div class="blur" style=${{ backgroundImage: 'url(' + art + ')' }} />`}
      ${art && html`<img src=${art} loading="lazy" alt="" onError=${e => e.target.remove()} />`}
    </div>
    <div class="meta">
      <div class="name">${g.title}</div>
      <${Bar} value=${pct(g.awarded, g.total)} thin />
      <div class="foot">
        <span>${g.awarded} / ${g.total}${g.hardcore > 0 && g.hardcore < g.awarded
          ? html` <span class="mute">${t('({n} hc)', { n: g.hardcore })}</span>` : ''}</span>
        <span class="right">${awardBadge(g) || html`<span class="mute">${g.console}</span>`}</span>
      </div>
    </div>
  </div>`;
}

const LIB_FILTERS = [['all', 'ALL {n}'], ['new', 'NOT STARTED'], ['progress', 'IN PROGRESS'],
                     ['beaten', 'BEATEN'], ['mastered', 'MASTERED']];

function LibraryTab() {
  useEffect(() => { loadLibrary(); }, []);
  const library = S.library.value;

  if (!library) return html`<div class="tab"><${Empty}>${t('loading your library...')}<//></div>`;
  if (library.error) return html`<div class="tab"><${Empty}>${t('could not reach the client')}<//></div>`;

  const filter = S.libFilter.value;
  const consoles = S.libConsoles.value;
  const open = S.libConsolesOpen.value;

  const isMastered = g => g.mastered || (g.total > 0 && g.awarded >= g.total);
  const shown = library.filter(g => {
    const ok =
      filter === 'all' ? true :
      filter === 'new' ? g.awarded === 0 :
      filter === 'progress' ? g.awarded > 0 && g.awarded < g.total && !(g.award || '').startsWith('beaten') :
      filter === 'beaten' ? (g.award || '').startsWith('beaten') :
      isMastered(g);
    return ok && (consoles.size === 0 || consoles.has(g.console));
  });

  /* Console chips: several can be on; an empty set means all. */
  const perConsole = {};
  for (const g of library) perConsole[g.console] = (perConsole[g.console] || 0) + 1;
  const names = Object.keys(perConsole).sort();
  const conLabel = consoles.size === 0 ? t('ALL CONSOLES')
    : t(consoles.size > 1 ? '{n} CONSOLES' : '{n} CONSOLE', { n: consoles.size });
  const toggleConsole = c => {
    const next = new Set(consoles);
    next.has(c) ? next.delete(c) : next.add(c);
    S.libConsoles.value = next;
  };

  return html`<div class="tab">
    <div class="filters">
      ${LIB_FILTERS.map(([k, label]) => html`<${Chip} key=${k} on=${filter === k}
        onClick=${() => { S.libFilter.value = k; }}>${t(label, { n: library.length })}<//>`)}
      <${Chip} on=${consoles.size > 0} onClick=${() => { S.libConsolesOpen.value = !open; }}>
        ${conLabel} ${open ? '▴' : '▾'}<//>
    </div>
    ${open && html`<div class="filters">
      ${names.map(c => html`<${Chip} key=${c} on=${consoles.has(c)} n=${perConsole[c]}
        onClick=${() => toggleConsole(c)}>${c.toUpperCase()}<//>`)}
    </div>`}
    <div class="grid">${shown.map(g => html`<${GameCard} key=${g.id} g=${g} />`)}</div>
    ${!shown.length && html`<${Empty}>${t('nothing matches these filters')}<//>`}
  </div>`;
}
