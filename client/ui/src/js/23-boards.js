/* ---- BOARDS: leaderboards, with the live trackers on top --------------- */

function BoardsTab() {
  const id = S.gameId.value;
  S.boardsTick.value; /* re-render when a fetch lands */
  useEffect(() => { loadBoards(id); }, [id]);

  if (!id) return html`<div class="tab"><${Empty}>${t('pick a game in the library')}<//></div>`;
  const b = S.boardsCache[id];
  if (!b || b.loading) return html`<div class="tab"><${Empty}>${t('loading...')}<//></div>`;
  if (b.error) return html`<div class="tab"><${Empty}>${t('could not reach the client')}<//></div>`;

  const s = S.state.value;
  const tracking = (s && s.game.tracking) || [];
  const open = S.openBoards.value;
  const toggle = bid => {
    const next = new Set(open);
    next.has(bid) ? next.delete(bid) : next.add(bid);
    S.openBoards.value = next;
  };

  return html`<div class="tab">
    ${tracking.length > 0 && html`<${Eyebrow}>${t('TRACKING NOW &middot; FROM CONSOLE MEMORY')}<//>
      ${tracking.map(tr => html`<div class="row near" key=${tr.id}>
        <div class="body"><div class="t">${tr.title}</div></div>
        <div class="m"><span class="lead acc">${trackVal(tr.id)}</span></div>
      </div>`)}`}
    <${Eyebrow}>${t('{n} LEADERBOARDS &middot; CLICK ONE TO EXPAND', { n: b.boards.length })}<//>
    ${b.boards.map(bd => {
      const isOpen = open.has(bd.id);
      return html`<div class=${'row clickable' + (isOpen ? ' near' : '')} key=${bd.id} onClick=${() => toggle(bd.id)}>
        <div class="body">
          <div class="t">${isOpen ? '▾ ' : '▸ '}${bd.title}</div>
          ${isOpen && html`<div class="d">${bd.description}</div>`}
        </div>
        <div class="m">
          ${bd.topScore ? html`<span class="lead acc">${bd.topScore}</span>${isOpen && html`<span class="tag">${bd.topUser}</span>`}`
                        : html`<span>${t('no entries')}</span>`}
        </div>
      </div>`;
    })}
  </div>`;
}
