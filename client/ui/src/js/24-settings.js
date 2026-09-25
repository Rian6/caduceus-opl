/* ---- SETTINGS: account, Web API key, the network switch ---------------- */

/* The tab re-renders on this key: the login and the network setting,
   not the whole state. */
const settingsKey = computed(() => {
  const s = S.state.value;
  if (!s) return '';
  const li = s.login || {}, lan = s.lan || {};
  return [li.ok, li.user, li.webapi, lan.on, lan.url].join('|');
});

function Msg({ text, good }) {
  return html`<div class=${'msg ' + (good ? 'ok' : 'warn')}>${text}</div>`;
}

function AccountBlock({ li }) {
  const [msg, setMsg] = useState(['', true]);
  const [busy, setBusy] = useState(false);

  const signIn = async e => {
    e.preventDefault();
    const form = e.target;
    setBusy(true); setMsg([t('signing in...'), true]);
    try {
      const r = await postForm('/login', { user: form.user.value.trim(), password: form.password.value });
      if (r.ok) { setMsg(['', true]); await refreshState(); }
      else setMsg([r.error || t('failed'), false]);
    } catch (err) { setMsg([t('could not reach the client'), false]); }
    setBusy(false);
  };
  const signOut = async () => {
    try { await postForm('/logout', {}); } catch (err) { /* gone already */ }
    try { await refreshState(); } catch (err) { /* the poll catches up */ }
  };

  return html`<div class="block">
    <${Eyebrow}>${t('RETROACHIEVEMENTS ACCOUNT')}<//>
    ${li.ok
      ? html`<div>${tv('signed in as {user}', { user: html`<span class="ok">${li.user}</span>` })}</div>
          <div class="note">${t('unlocks from the console are credited to this account')}</div>
          <${Btn} onClick=${signOut}>${t('SIGN OUT')}<//>`
      : html`<div class="note">${t('the account your unlocks are credited to; only a login token is stored')}</div>
          <form onSubmit=${signIn}>
            <label>${t('username')}</label><input class="f" name="user" autocomplete="username" />
            <label>${t('password')}</label><input class="f" name="password" type="password" autocomplete="current-password" />
            <${Btn} type="submit" primary disabled=${busy}>${t('SIGN IN')}<//>
          </form>`}
    <${Msg} text=${msg[0]} good=${msg[1]} />
  </div>`;
}

function KeyBlock({ li }) {
  const [msg, setMsg] = useState(['', true]);
  const [busy, setBusy] = useState(false);

  const save = async e => {
    e.preventDefault();
    const form = e.target;
    setBusy(true); setMsg([t('saving...'), true]);
    try {
      const r = await postForm('/apikey', { key: form.key.value.trim() });
      if (r.ok) {
        form.key.value = '';
        S.library.value = null;
        loadProfile();
        await refreshState();
        setMsg([t('saved'), true]);
      } else setMsg([r.error || t('failed'), false]);
    } catch (err) { setMsg([t('could not reach the client'), false]); }
    setBusy(false);
  };

  return html`<div class="block">
    <${Eyebrow}>${t('WEB API KEY')}<//>
    <div class="note">${tv('a read-only key that fills the library, leaderboards and profile. Get it at {link}, section "Keys", and paste it here.',
      { link: html`<a href="https://retroachievements.org/settings" target="_blank" rel="noopener">retroachievements.org/settings</a>` })}</div>
    <form onSubmit=${save}>
      <label>${t('key')} ${li.webapi && html`<span class="ok">${t('(saved, paste to replace)')}</span>`}</label>
      <input class="f" name="key" autocomplete="off" />
      <${Btn} type="submit" primary disabled=${busy}>${t('SAVE KEY')}<//>
    </form>
    <${Msg} text=${msg[0]} good=${msg[1]} />
  </div>`;
}

function LanBlock({ lan }) {
  const [msg, setMsg] = useState(['', true]);
  const [busy, setBusy] = useState(false);

  const flip = async () => {
    setBusy(true); setMsg([t(lan.on ? 'closing...' : 'opening...'), true]);
    try {
      const r = await postForm('/lan', { on: lan.on ? '0' : '1' });
      if (!r.ok) { setMsg([r.error || t('failed'), false]); setBusy(false); return; }
      /* The listener reopens on the next request. */
      await new Promise(res => setTimeout(res, 700));
      await refreshState();
      setMsg(['', true]);
    } catch (err) { setMsg([t('could not reach the client'), false]); }
    setBusy(false);
  };

  return html`<div class="block">
    <${Eyebrow}>${t('OTHER DEVICES')}<//>
    ${lan.on
      ? html`<div>${t('open to the network')}${lan.url
            ? html`: <a class="ok" href=${lan.url}>${lan.url}</a>`
            : ' ' + t('(no network address found yet)')}</div>
          <div class="note">${t('type that address into a phone or tablet on the same Wi-Fi and add the page to the home screen. Other devices watch; settings and QUIT stay here. On Windows, allow xerabora through the firewall when it asks.')}</div>
          <${Btn} onClick=${flip} disabled=${busy}>${t('CLOSE TO THIS PC')}<//>`
      : html`<div class="note">${t('the page is reachable from this PC only. Open it to the network and any phone, tablet or second PC on the same Wi-Fi can watch this tracker.')}</div>
          <${Btn} onClick=${flip} primary disabled=${busy}>${t('OPEN TO THE NETWORK')}<//>`}
    <${Msg} text=${msg[0]} good=${msg[1]} />
  </div>`;
}

function SettingsTab() {
  settingsKey.value;
  const s = S.state.peek();
  const li = (s && s.login) || { ok: false, user: '', webapi: false };
  const lan = (s && s.lan) || { on: false, url: '' };

  if (REMOTE) {
    return html`<div class="tab"><div class="set">
      <${Eyebrow}>${t('OTHER DEVICES')}<//>
      <div>${tv('this page is open from {host}', { host: html`<span class="ok">${location.host}</span>` })}</div>
      <div class="note">${t('account, Web API key, the network switch and QUIT are changed on the PC that runs xerabora; from here you watch.')}</div>
      ${li.ok && html`<div style=${{ marginTop: '14px' }}>${tv('signed in as {user}', { user: html`<span class="ok">${li.user}</span>` })}</div>`}
    </div></div>`;
  }

  return html`<div class="tab"><div class="set">
    <${AccountBlock} li=${li} />
    <${KeyBlock} li=${li} />
    <${LanBlock} lan=${lan} />
  </div></div>`;
}
