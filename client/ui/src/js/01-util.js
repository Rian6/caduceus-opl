/* ---- Small helpers, shared by every tab -------------------------------- */

const BADGE = 'https://media.retroachievements.org/Badge/';
const MEDIA = 'https://media.retroachievements.org';

/* True when the page was opened from another device. Settings, the
   follow switch and QUIT are then hidden. */
const REMOTE = !['127.0.0.1', 'localhost', '::1', '[::1]'].includes(location.hostname);

const TABS = ['live', 'library', 'game', 'boards', 'settings'];

function hms(s) {
  const h = Math.floor(s / 3600), m = Math.floor(s % 3600 / 60);
  return h + ':' + String(m).padStart(2, '0') + ':' + String(s % 60).padStart(2, '0');
}

function pct(a, b) { return b ? Math.round(a * 100 / b) : 0; }

/* rc_client's measured_percent is already 0..100. */
function pctClamp(v) { return Math.max(0, Math.min(100, +v || 0)); }

function ago(sec) {
  if (sec < 60) return t('{n} s', { n: sec });
  if (sec < 3600) return t('{n} min', { n: Math.floor(sec / 60) });
  return t('{h} h {m} min', { h: Math.floor(sec / 3600), m: Math.floor((sec % 3600) / 60) });
}

/* The whole line is one dictionary string. */
function agoText(sec) { return t('{time} ago', { time: ago(sec) }); }

function medianText(s) {
  return Math.floor(s / 3600) + 'h' + String(Math.floor(s % 3600 / 60)).padStart(2, '0');
}

function badgeUrl(a, locked) { return BADGE + a.badge + (locked ? '_lock' : '') + '.png'; }

/* Rank in the progression-path order; untyped rows take 2. */
const TYPE_RANK = { progression: 0, win_condition: 1, missable: 3 };

/* achType() maps rc_client's numeric type or the Web API's string to
   'missable', 'progression', 'win_condition' or ''. */
const TYPE_NAMES = ['', 'missable', 'progression', 'win_condition'];

function achType(a) {
  return typeof a.type === 'number' ? (TYPE_NAMES[a.type] || '')
                                    : (a.type || '').toLowerCase();
}

function onPath(a) {
  const ty = achType(a);
  return ty === 'progression' || ty === 'win_condition';
}

/* Time cost of a set from its medians: beaten is the latest median on
   the intended path, full the latest of all. */
function setEstimates(list, medOf) {
  const meds = list.map(medOf).filter(m => m > 0);
  const path = list.filter(onPath).map(medOf).filter(m => m > 0);

  return { full: meds.length ? Math.max(...meds) : 0,
           beaten: path.length ? Math.max(...path) : 0 };
}

/* UP NEXT: the four locked achievements with the lowest medians. The
   first locked progression or win-condition achievement takes the last
   slot when it is not among them. */
function upNextPick(list, med) {
  const byMed = (x, y) => med(x) - med(y);
  const locked = list.filter(a => a.state === 1 && med(a) > 0);
  const spine = locked.filter(onPath).sort(byMed);
  const next = locked.sort(byMed).slice(0, 4);

  if (spine.length && !next.includes(spine[0])) {
    next[next.length - 1] = spine[0];
    next.sort(byMed);
  }
  return { next, prog: spine.length ? spine[0] : null };
}

/* Locked missables whose median is at or before the latest median
   among the unlocked progression and win-condition achievements. */
function possiblyMissed(list, med) {
  const passed = Math.max(0, ...list.filter(a => a.state === 2 && onPath(a)).map(med));
  const out = new Set();

  for (const a of list)
    if (a.state === 1 && achType(a) === 'missable' && med(a) > 0 && med(a) <= passed)
      out.add(a.id);
  return out;
}

function sortAchievements(list, order) {
  const byAuthor = (a, b) => a.order - b.order;

  if (order === 'median')
    /* Never-unlocked-by-anyone (median 0) goes to the end. */
    return list.sort((a, b) =>
      (a.median > 0 ? a.median : Infinity) - (b.median > 0 ? b.median : Infinity) || byAuthor(a, b));
  if (order === 'type')
    return list.sort((a, b) =>
      (TYPE_RANK[achType(a)] ?? 2) - (TYPE_RANK[achType(b)] ?? 2) || byAuthor(a, b));
  return list.sort(byAuthor);
}
