/* ---- LIVE: the console's session, or the account's play elsewhere ------ */

const STATUS = {
  'active':         ['achievement set active, unlocks are tracked', 'ok'],
  'no-hash':        ["image not identified &mdash; run 'RA: check game support' in the game's OPL menu", 'warn'],
  'telemetry-only': ['telemetry only &mdash; no achievement set could be loaded for this image', 'warn'],
  'identifying':    ['loading the achievement set for this image &mdash; a few seconds', 'warn'],
  'stale':          ["console watch list is out of date &mdash; run 'RA: check game support' again", 'warn']
};

/* Frame counter with the rate, or the packet count before the first frame. */
const framesText = computed(() => {
  const fr = fast.frames.value, r = fast.rate.value;
  return fr ? fr.toLocaleString() + (r ? ' · ' + r + '/s' : '')
            : t('{packets} packets', { packets: fast.packets.value.toLocaleString() });
});
const sessionText = computed(() => fast.seconds.value ? hms(fast.seconds.value) : '—');
const fSinceText = computed(() => fast.fSince.value ? hms(fast.fSince.value) : '—');
const fPolledText = computed(() => fast.fPolled.value >= 0 ? agoText(fast.fPolled.value) : '—');

/* FOLLOW MY PLAY: hidden on REMOTE and without a Web API key. Posts
   /follow and fetches the state again. */
function FollowSwitch({ s }) {
  const [busy, setBusy] = useState(false);
  const [err, setErr] = useState('');
  const f = s.follow || {}, li = s.login || {};

  if (REMOTE || !li.webapi) return null;
  const text = err ? err
    : s.console.connected ? t('the console has LIVE while it streams; following takes over when it stops')
    : f.on ? t('LIVE shows the game you are in on any emulator with RetroAchievements, and the unlocks as they land')
           : t('off. Turn it on and LIVE follows your account on any emulator with RetroAchievements');

  const toggle = async () => {
    setBusy(true); setErr('');
    try {
      const r = await postForm('/follow', { on: f.on ? '0' : '1' });
      if (r.ok) await refreshState(); else setErr(r.error || t('failed'));
    } catch (e) { setErr(t('could not reach the client')); }
    setBusy(false);
  };
  return html`<${Switch} on=${!!f.on} label=${t('FOLLOW MY PLAY')} text=${text} busy=${busy} onToggle=${toggle} />`;
}

/* One line per counter in a tile: the string is split on its dots. */
function lines(text) {
  return text.split(' \u00b7 ').map((p, i) => html`${i ? html`<br />` : ''}${p}`);
}

/* The three loss counters; the component redraws when any one moves. */
function Lost() {
  return lines(t('{gaps} skipped \u00b7 {dupes} repeated \u00b7 {torn} torn',
                 { gaps: fast.gaps.value, dupes: fast.dupes.value, torn: fast.torn.value }));
}

/* CONSOLE panel: link, serial, counters, watch list, session clock. */
function ConsolePanel({ s }) {
  const c = s.console;
  if (!c.connected && !c.packets) return null;
  if (!c.connected && (s.follow || {}).active) return null;

  const st = STATUS[c.status];
  const w = c.watch || {};
  const watch = w.addresses
    ? lines(t('{addresses} addr &middot; {bytes} B &middot; {parts} parts',
              { addresses: w.addresses, bytes: w.bytes, parts: w.parts }))
    : '—';

  return html`<${Fragment}>
    <${Eyebrow}>${t('CONSOLE')}<//>
    ${st && html`<div class=${'banner ' + st[1]}>${t(st[0])}</div>`}
    <div class="stats">
      <${Cell} k=${t('LINK')} v=${c.connected ? c.ip : html`<span class="mute">${t('lost')}</span>`} />
      <${Cell} k=${t('GAME')} v=${s.game.serial || '—'} />
      <${Cell} k=${t('SNAPSHOTS')} v=${framesText} />
      <${Cell} k=${t('LOST')} v=${html`<${Lost} />`} />
      <${Cell} k=${t('WATCH LIST')} v=${watch} />
      <${Cell} k=${t('SESSION')} v=${sessionText} />
    </div>
    ${s.game.hash && html`<div class="hash">${t('hash {hash}', { hash: s.game.hash })}</div>`}
  <//>`;
}

/* FOLLOWING panel: the rich presence line, the poll, this session's unlocks. */
function FollowPanel({ s }) {
  const f = s.follow;
  if (!f || !f.active) return null;
  const seen = f.online ? html`<span class="ok">${t('playing now')}</span>`
             : f.rp_date ? f.rp_date
             : fast.fSeen.value >= 0 ? agoText(fast.fSeen.value) : '—';
  const session = f.session || [];

  return html`<${Fragment}>
    <${Eyebrow}>${t('FOLLOWING')}<//>
    <div class="banner ok">${f.rp ? f.rp : t('no rich presence line yet')}</div>
    <div class="stats">
      <${Cell} k=${t('PLATFORM')} v=${s.game.console || '—'} />
      <${Cell} k=${t('LAST SEEN')} v=${seen} />
      <${Cell} k=${t('SOURCE')} v=${t('RetroAchievements &middot; every 20 s')} />
      <${Cell} k=${t('POLLED')} v=${fPolledText} />
      <${Cell} k=${t('SESSION')} v=${fSinceText} />
      <${Cell} k=${t('UNLOCKS')} v=${session.length} />
    </div>
    ${f.failures ? html`<div class="note warn">${t('the Web API is not answering; retrying')}</div>` : null}
    ${session.length > 0 && html`<${Eyebrow}>${t('THIS SESSION')}<//>
      ${session.map(u => html`<div class="row done" key=${u.id}>
        <img class="badge" src=${BADGE + u.badge + '.png'} alt="" />
        <div class="body"><div class="t">${u.title}</div>
          <div class="d">${agoText(u.ago)}${u.hardcore ? ' · ' + t('hardcore') : ''}</div></div>
        <div class="m"><span class="lead ok">+${u.points}</span></div>
      </div>`)}`}
  <//>`;
}

/* The game's head: cover, title, progress bar, counts. */
function LiveHead({ s, list, done, ptsDone, ptsAll, icon }) {
  return html`<div class="head">
    <${Cover} src=${icon ? MEDIA + icon : ''} />
    <div class="body">
      <h1>${s.game.title || s.game.serial}</h1>
      <${Bar} value=${pct(done, list.length)} />
      <div class="sub">
        <span>${s.game.checked_only ? t('from an image check, not from a running game') : (s.game.serial || s.game.console || '')}</span>
        <span class="right"><b>${done}</b> / ${list.length} <span class="mute">·</span> <span class="gold">${ptsDone}</span><span class="mute"> / ${t('{points} pts', { points: ptsAll })}</span></span>
      </div>
    </div>
  </div>`;
}

/* ALL ACHIEVEMENTS: one list, or one per subset when there are several. */
function AchievementGroups({ list, subsets, row }) {
  if (!subsets || subsets.length < 2) return list.map(row);
  return subsets.map(sub => {
    const mine = list.filter(a => a.subset === sub.id);
    if (!mine.length) return null;
    const done = mine.filter(a => a.state === 2).length;
    return html`<${Fragment} key=${sub.id}>
      <${Eyebrow} count=${done + ' / ' + mine.length}>${sub.title.toUpperCase()}<//>
      ${mine.map(row)}
    <//>`;
  });
}

/* One row of ALL ACHIEVEMENTS; reads the achievement's live signal for
   the measured value and the progress. */
function LiveRow({ a, missed }) {
  const isDone = a.state === 2;
  /* state 3 is rc_client's disabled state: 'not on hardware'. */
  const na = a.state === 3;
  const lv = liveVal(a.id).value;
  const near = !isDone && !na && lv.p > 0;
  const state = isDone ? 'done' : na ? 'na' : near ? 'near' : 'locked';
  return html`<${AchRow} a=${a} state=${state}
    below=${!isDone && !na && html`<${Mini} id=${a.id} />`}
    right=${html`<${Fragment}>
      ${isDone ? html`<span class="lead ok">${t('unlocked')}</span>`
        : na ? html`<span class="lead warn">${t('not on hardware')}</span>`
        : lv.m ? html`<span class="lead acc">${lv.m}</span>`
        : html`<span class="lead">${t('locked')}</span>`}
      <${Pts} a=${a} extra=${missed ? html`<span class="warn">${t('possibly skipped')}</span>` : null} />
    <//>`} />`;
}

function LiveTab() {
  const s = S.state.value;
  if (!s) return html`<${Empty}>${t('loading...')}<//>`;

  const list = s.game.achievements || [];
  const f = s.follow || {};

  if (!list.length) {
    return html`<div class="tab">
      <${FollowSwitch} s=${s} /><${ConsolePanel} s=${s} />
      <${Empty}>${s.console.connected
        ? t('console connected, waiting for a game with achievements')
        : f.on && (s.login || {}).webapi
          ? t('following your RetroAchievements activity: start a game on the console, or in any emulator with RetroAchievements on, and it shows up here within half a minute')
          : t('start a game on the console, or browse your library above')}<//>
    </div>`;
  }

  const done = list.filter(a => a.state === 2).length;
  const ptsDone = list.filter(a => a.state === 2).reduce((n, a) => n + (a.points || 0), 0);
  const ptsAll = list.reduce((n, a) => n + (a.points || 0), 0);

  const lm = S.liveMeta.value;
  const meta = lm.id === s.game.id ? lm.byId : null;
  const medOf = a => meta ? ((meta[a.id] || {}).median || 0) : 0;
  const missed = meta ? possiblyMissed(list, medOf) : new Set();
  const position = meta ? Math.max(0, ...list.filter(a => a.state === 2).map(medOf)) : 0;
  const est = meta ? setEstimates(list, medOf) : null;

  /* UP NEXT: the nearest locked achievements by median, drawn big. */
  let ahead = null;
  if (meta) {
    const picked = upNextPick(list, medOf);
    if (picked.next.length) {
      ahead = html`<${Eyebrow}>${t('UP NEXT &middot; BY MEDIAN UNLOCK TIME')}<//>
        ${picked.next.map(a => html`<${AchRow} key=${a.id} a=${a} state="near" big
          below=${html`<${Mini} id=${a.id} />`}
          right=${html`<${Fragment}>
            <${Measured} id=${a.id} />
            ${a === picked.prog && html`<span class="tag acc">${t('next in progression')}</span>`}
            ${missed.has(a.id) ? html`<span class="tag warn">${t('possibly skipped')}</span>`
              : medOf(a) <= position ? html`<span class="tag warn">${t('usually done by now')}</span>` : null}
            <${Pts} a=${a} extra=${'~' + medianText(medOf(a))} />
          <//>`} />`)}`;
    }
  }

  const trackers = (s.game.tracking || []).length
    ? html`<${Eyebrow}>${t('TRACKING NOW')}<//>
        ${s.game.tracking.map(tr => html`<div class="row near" key=${tr.id}>
          <div class="body"><div class="t">${tr.title}</div></div>
          <div class="m"><span class="lead acc">${trackVal(tr.id)}</span></div>
        </div>`)}`
    : null;

  const strip = html`<div class="stats">
    <${Cell} k=${t('ACHIEVEMENTS')} v=${done + ' / ' + list.length} />
    <${Cell} k=${t('POINTS')} v=${html`<span class="gold">${ptsDone}</span> / ${ptsAll}`} />
    <${Cell} k=${t('BEATEN AT')} v=${est && est.beaten ? '~' + medianText(est.beaten) : '—'} />
    <${Cell} k=${t('FULL SET AT')} v=${est && est.full ? '~' + medianText(est.full) : '—'} />
    <${Cell} k=${t('YOUR POSITION')} v=${position ? '~' + medianText(position) : '—'} />
    <${Cell} k=${t('MISSABLES LEFT')} v=${list.filter(a => a.state !== 2 && achType(a) === 'missable').length} />
  </div>`;

  const row = a => html`<${LiveRow} key=${a.id} a=${a} missed=${missed.has(a.id)} />`;

  const showAll = ahead || trackers || (s.game.subsets || []).length > 1;

  return html`<div class="tab">
    <${FollowSwitch} s=${s} />
    <${FollowPanel} s=${s} />
    <${ConsolePanel} s=${s} />
    <${LiveHead} s=${s} list=${list} done=${done} ptsDone=${ptsDone} ptsAll=${ptsAll}
      icon=${(meta && lm.icon) || s.game.icon || ''} />
    ${strip}
    ${ahead}
    ${trackers}
    ${showAll && html`<${Eyebrow} count=${done + ' / ' + list.length}>${t('ALL ACHIEVEMENTS')}<//>`}
    <${AchievementGroups} list=${list} subsets=${s.game.subsets} row=${row} />
  </div>`;
}
