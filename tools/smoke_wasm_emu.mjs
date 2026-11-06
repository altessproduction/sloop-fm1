#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-only
// Browser-level smoke test: boot the real firmware, verify LCD activity and
// measure non-silent AudioWorklet output after pressing a key.
import { chromium } from 'playwright';

const errors = [];
const browser = await chromium.launch({
  headless: true,
  args: ['--autoplay-policy=no-user-gesture-required', '--enable-features=AudioWorklet'],
});
try {
  const page = await browser.newPage({ viewport: { width: 1440, height: 1000 } });
  page.on('pageerror', e => errors.push(String(e)));
  page.on('console', msg => { if (msg.type() === 'error') errors.push(msg.text()); });
  await page.addInitScript(() => {
    const original = AudioWorkletNode.prototype.connect;
    AudioWorkletNode.prototype.connect = function(destination, ...args) {
      const analyser = this.context.createAnalyser();
      analyser.fftSize = 2048;
      original.call(this, analyser);
      analyser.connect(destination);
      window.__sloopAnalyser = analyser;
      return destination;
    };
  });
  await page.goto('http://127.0.0.1:8787/', { waitUntil: 'networkidle' });
  await page.locator('#power').waitFor();
  await page.locator('#power').click();
  await page.waitForFunction(() => document.querySelector('#power') === null);
  await page.waitForTimeout(1800);

  const screenState = () => page.evaluate(() => {
    const c = document.querySelector('#screen');
    if (!c) throw new Error('FM-1 LCD canvas #screen is missing');
    const d = c.getContext('2d').getImageData(0, 0, c.width, c.height).data;
    let lit = 0, hash = 2166136261;
    for (let i = 0; i < d.length; i += 4) {
      if (d[i] || d[i + 1] || d[i + 2]) lit++;
      hash = Math.imul(hash ^ d[i], 16777619);
      hash = Math.imul(hash ^ d[i + 1], 16777619);
      hash = Math.imul(hash ^ d[i + 2], 16777619);
    }
    return { lit, hash: hash >>> 0 };
  });
  const home = await screenState();
  if (home.lit < 100) throw new Error(`LCD did not render firmware UI (lit pixels: ${home.lit})`);

  const key = page.locator('.pill').first();
  await key.waitFor();
  await key.dispatchEvent('pointerdown', { pointerId: 1, pointerType: 'mouse', isPrimary: true, button: 0, buttons: 1 });
  await page.waitForTimeout(900);
  const rms = await page.evaluate(() => {
    const a = window.__sloopAnalyser;
    if (!a) throw new Error('AudioWorklet output was not connected to the analyser');
    const data = new Float32Array(a.fftSize);
    a.getFloatTimeDomainData(data);
    return Math.sqrt(data.reduce((sum, v) => sum + v * v, 0) / data.length);
  });
  await key.dispatchEvent('pointerup', { pointerId: 1, pointerType: 'mouse', isPrimary: true, button: 0, buttons: 0 });
  if (!(rms > 0.00001)) throw new Error(`AudioWorklet output is silent (RMS ${rms})`);

  const glo = page.locator('.btn').filter({ hasText: 'GLO' }).first();
  await glo.dispatchEvent('pointerdown', { pointerId: 2, pointerType: 'mouse', isPrimary: true, button: 0, buttons: 1 });
  await page.waitForTimeout(350);
  const mixer = await screenState();
  await glo.dispatchEvent('pointerup', { pointerId: 2, pointerType: 'mouse', isPrimary: true, button: 0, buttons: 0 });
  await page.waitForTimeout(350);
  const returned = await screenState();
  if (mixer.hash === home.hash) throw new Error('GLO did not open a distinct firmware screen');
  if (returned.hash !== home.hash) throw new Error('Releasing GLO did not return to the previous firmware screen');
  if (errors.length) throw new Error('Browser errors: ' + errors.join(' | '));
  console.log(JSON.stringify({ ok: true, litPixels: home.lit, audioRms: rms, gloLayer: true, gloExit: true }));
} finally {
  await browser.close();
}
