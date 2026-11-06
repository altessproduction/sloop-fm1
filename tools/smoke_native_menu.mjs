#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-only
// Verify the actual FM-1 LCD/menu path in the browser, not an external control panel.
import { chromium } from 'playwright';

const errors = [];
const browser = await chromium.launch({headless:true,args:['--autoplay-policy=no-user-gesture-required']});
try {
  const page = await browser.newPage({viewport:{width:1440,height:1000}});
  page.on('pageerror', e => errors.push(String(e)));
  page.on('console', m => { if(m.type()==='error' && m.location().url.startsWith('http://127.0.0.1:8787/')) errors.push(m.text()); });
  await page.goto('http://127.0.0.1:8787/',{waitUntil:'networkidle'});
  await page.locator('#power').click();
  await page.waitForFunction(()=>!document.querySelector('#power'));
  await page.waitForTimeout(1200);

  if(await page.locator('#roots-controls').count()!==0)
    throw new Error('ROOTS DUB controls must not be outside the FM-1 faceplate');
  const lcdHash=()=>page.locator('#screen').evaluate(c=>{
    const d=c.getContext('2d').getImageData(0,0,c.width,c.height).data;
    let h=2166136261;
    for(let i=0;i<d.length;i+=4){h=Math.imul(h^d[i],16777619);h=Math.imul(h^d[i+1],16777619);h=Math.imul(h^d[i+2],16777619);}
    return h>>>0;
  });
  const homeHash=await lcdHash();
  const home=page.locator('.btn').filter({hasText:/HOME/}).first();
  await home.dispatchEvent('pointerdown',{pointerId:1,pointerType:'mouse',isPrimary:true,button:0,buttons:1});
  await page.waitForTimeout(300);
  const menuHash=await lcdHash();
  if(menuHash===homeHash) throw new Error('Holding the native HOME button did not open the firmware menu');

  // SELECT is encoder 0. The native menu has SCREEN, LIGHTS, AUDIO, SEQ,
  // SYSTEM, ROOTS, DUB PAT, DUB PLAY, DUB BASS and DUB SKANK sections.
  const select=page.locator('.cap').nth(0);
  const hashes=[];
  for(let i=0;i<5;i++){
    await select.dispatchEvent('wheel',{deltaY:-120});
    await page.waitForTimeout(100);
    hashes.push(await lcdHash());
  }
  if(hashes.every(h=>h===menuHash)) throw new Error('Native menu section selector did not update the LCD');
  await home.dispatchEvent('pointerup',{pointerId:1,pointerType:'mouse',isPrimary:true,button:0,buttons:0});
  await page.waitForTimeout(250);

  if(errors.length) throw new Error('Browser errors: '+errors.join(' | '));
  console.log(JSON.stringify({ok:true,nativeHomeMenu:true,selectEncoder:true,lcdChangedSections:hashes.length}));
} finally { await browser.close(); }
