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
  page.on('console', msg => { if (msg.type() === 'error' && msg.location().url.startsWith('http://127.0.0.1:8787/')) errors.push(msg.text()); });
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
  // GLO is a performance layer: it changes the key/button LEDs, not necessarily
  // the LCD pixels. Test the actual panel feedback rather than expecting a new page.
  const ledState = () => page.evaluate(() =>
    [...document.querySelectorAll('.pill, .btn')]
      .map(el => el.classList.contains('lit') ? 'lit' : el.classList.contains('glow') ? 'glow' : '-')
      .join(',')
  );
  const muteLedState = () => page.locator('.pill').first().evaluate(el =>
    el.classList.contains('lit') ? 'lit' : el.classList.contains('glow') ? 'glow' : '-'
  );
  const home = await screenState();
  const homeLeds = await ledState();
  if (home.lit < 100) throw new Error(`LCD did not render firmware UI (lit pixels: ${home.lit})`);

  const key = page.locator('.pill').first();
  await key.waitFor();
  await key.dispatchEvent('pointerdown', { pointerId: 1, pointerType: 'mouse', isPrimary: true, button: 0, buttons: 1 });
  await page.waitForTimeout(900);
  const measureRms = () => page.evaluate(() => {
    const a = window.__sloopAnalyser;
    if (!a) throw new Error('AudioWorklet output was not connected to the analyser');
    const data = new Float32Array(a.fftSize);
    a.getFloatTimeDomainData(data);
    return Math.sqrt(data.reduce((sum, v) => sum + v * v, 0) / data.length);
  });
  const rms = await measureRms();
  if (!(rms > 0.00001)) throw new Error(`AudioWorklet output is silent (RMS ${rms})`);

  // The factory patch can decay naturally while a key remains held, so a
  // 15-second "sustain" assertion is invalid. Re-trigger a real key once per
  // second and measure after each attack/release cycle instead.
  await key.dispatchEvent('pointerup', { pointerId: 1, pointerType: 'mouse', isPrimary: true, button: 0, buttons: 0 });
  const activityRms = [];
  for (let i = 0; i < 15; i++) {
    const pointerId = i + 2;
    await key.dispatchEvent('pointerdown', { pointerId, pointerType: 'mouse', isPrimary: true, button: 0, buttons: 1 });
    await page.waitForTimeout(180);
    activityRms.push(await measureRms());
    await key.dispatchEvent('pointerup', { pointerId, pointerType: 'mouse', isPrimary: true, button: 0, buttons: 0 });
    await page.waitForTimeout(820);
  }
  const minActivityRms = Math.min(...activityRms);
  const silentWindows = activityRms.filter(v => !(v > 0.00001)).length;
  if (silentWindows) throw new Error(`AudioWorklet had ${silentWindows} silent retrigger measurements (minimum RMS ${minActivityRms})`);

  await page.keyboard.down('6');                 // Hold GLO, then HOME locks the layer open.
  await page.waitForTimeout(80);
  await page.keyboard.down('7');
  await page.waitForTimeout(80);
  await page.keyboard.up('7');
  await page.waitForTimeout(80);
  await page.keyboard.up('6');
  await page.waitForTimeout(500);
  const mixer = await screenState();
  const mixerLeds = await ledState();
  const muteBefore = await muteLedState();
  if (mixerLeds === homeLeds) throw new Error('GLO + HOME did not change the panel LEDs to the locked GLO layer');

  // In GLO, the first white key toggles MUTE 1. Toggle it twice so the
  // smoke test verifies the real action without leaving the saved project changed.
  await key.dispatchEvent('pointerdown', { pointerId: 2, pointerType: 'mouse', isPrimary: true, button: 0, buttons: 1 });
  await page.waitForTimeout(120);
  await key.dispatchEvent('pointerup', { pointerId: 2, pointerType: 'mouse', isPrimary: true, button: 0, buttons: 0 });
  await page.waitForTimeout(250);
  const mutedLed = await muteLedState();
  if (mutedLed === muteBefore) throw new Error('GLO key 1 did not change MUTE 1 feedback');
  await key.dispatchEvent('pointerdown', { pointerId: 3, pointerType: 'mouse', isPrimary: true, button: 0, buttons: 1 });
  await page.waitForTimeout(120);
  await key.dispatchEvent('pointerup', { pointerId: 3, pointerType: 'mouse', isPrimary: true, button: 0, buttons: 0 });
  await page.waitForTimeout(250);
  if (await muteLedState() !== muteBefore) throw new Error('GLO MUTE 1 did not toggle back to its original state');

  await page.keyboard.down('7');                 // HOME unlocks the GLO layer.
  await page.waitForTimeout(80);
  await page.keyboard.up('7');
  await page.waitForTimeout(500);
  const returned = await screenState();
  const returnedLeds = await ledState();
  if (returnedLeds !== homeLeds) throw new Error('HOME did not restore the normal panel LEDs after GLO');
  if (errors.length) throw new Error('Browser errors: ' + errors.join(' | '));
  console.log(JSON.stringify({ ok: true, litPixels: home.lit, audioRms: rms, retriggerSeconds: 15, minActivityRms, silentWindows, gloLayerLocked: true, homeUnlock: true }));
} finally {
  await browser.close();
}
