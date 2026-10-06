// Run the integrated server against its built UI without a Vite dependency.
const path = require('node:path');
if (!process.argv[2]) throw new Error('Usage: electron start-caduceus-local.cjs <server-directory>');
const root = path.resolve(process.argv[2]);
const {app, BrowserWindow} = require('electron');
app.setName('roms-desktop');
const loadURL = BrowserWindow.prototype.loadURL;
BrowserWindow.prototype.loadURL = function (url, options) {
  if (url === 'http://localhost:5173') {
    return this.loadFile(path.join(root, 'dist/index.html'));
  }
  return loadURL.call(this, url, options);
};
process.chdir(root);
require(path.join(root, 'dist-electron/main.js'));
