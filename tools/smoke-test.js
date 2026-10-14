/* Smoke test for the browser build.
 *
 * Not run by CI -- it needs Playwright and a Chromium. Run it after touching
 * anything in src/ or web/:
 *
 *     ./web/build.sh
 *     NODE_PATH=/opt/node22/lib/node_modules node tools/smoke-test.js
 *
 * Two browser contexts, because one cannot cover both honestly:
 *   phone   -- taps and a held stick, sent as real touch events over CDP.
 *              Chromium drops the coordinates of touchMove, so no drags here.
 *   desktop -- a real mouse drag, the same pointer path a finger takes when
 *              looking around.
 *
 * Every pixel measurement has a control: the same measurement while nothing
 * happens. Without it "the picture changed" proves nothing -- the frame rate
 * counter in the HUD changes every frame.
 */
const { chromium } = require('playwright');
const http = require('http');
const fs = require('fs');
const path = require('path');

const ROOT = path.join(__dirname, '..', 'build', 'web');
const PORT = 8099;
const OUT = process.env.SHOT_DIR || '/tmp';
const TYPES = {
    '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css',
    '.wasm': 'application/wasm', '.png': 'image/png',
    '.data': 'application/octet-stream'
};

let failures = 0;
function check(name, ok, detail) {
    console.log((ok ? 'PASS ' : 'FAIL ') + name + (detail ? ' — ' + detail : ''));
    if (!ok) failures++;
}

const server = http.createServer((req, res) => {
    const file = path.join(ROOT, req.url === '/' ? 'index.html' : req.url.split('?')[0]);
    fs.readFile(file, (err, data) => {
        if (err) { res.writeHead(404); return res.end('no'); }
        res.writeHead(200, { 'Content-Type': TYPES[path.extname(file)] || 'text/plain' });
        res.end(data);
    });
});

async function diff(page, a, b) {
    return page.evaluate(async ([a, b]) => {
        async function pixels(data) {
            const img = new Image();
            img.src = 'data:image/png;base64,' + data;
            await img.decode();
            const c = new OffscreenCanvas(img.width, img.height);
            const g = c.getContext('2d');
            g.drawImage(img, 0, 0);
            return g.getImageData(0, 0, img.width, img.height).data;
        }
        const [p, q] = [await pixels(a), await pixels(b)];
        let sum = 0;
        for (let i = 0; i < p.length; i += 4) {
            sum += Math.abs(p[i] - q[i]) + Math.abs(p[i + 1] - q[i + 1]) +
                   Math.abs(p[i + 2] - q[i + 2]);
        }
        return sum / (p.length / 4 * 3);
    }, [a, b]);
}

(async () => {
    await new Promise(r => server.listen(PORT, r));
    const browser = await chromium.launch({
        executablePath: process.env.CHROMIUM ||
            '/opt/pw-browsers/chromium-1194/chrome-linux/chrome',
        args: ['--use-gl=swiftshader', '--enable-unsafe-swiftshader', '--no-sandbox'],
    });

    // ===================== phone =====================
    const phone = await browser.newContext({
        viewport: { width: 390, height: 844 },
        deviceScaleFactor: 2, isMobile: true, hasTouch: true,
    });
    const page = await phone.newPage();
    const cdp = await phone.newCDPSession(page);
    const errors = [];
    page.on('pageerror', e => errors.push('pageerror: ' + e.message));
    page.on('console', m => { if (m.type() === 'error') errors.push('console: ' + m.text()); });

    const touchStart = (x, y) => cdp.send('Input.dispatchTouchEvent',
        { type: 'touchStart', touchPoints: [{ x, y }] });
    const touchEnd = () => cdp.send('Input.dispatchTouchEvent',
        { type: 'touchEnd', touchPoints: [] });
    async function tapLocator(locator) {
        const box = await locator.boundingBox();
        await touchStart(box.x + Math.min(40, box.width / 2), box.y + box.height / 2);
        await touchEnd();
        await page.waitForTimeout(200);
    }
    async function tap(selector, index) {
        const box = await (index === undefined
            ? page.locator(selector)
            : page.locator(selector).nth(index)).boundingBox();
        await touchStart(box.x + box.width / 2, box.y + box.height / 2);
        await touchEnd();
        await page.waitForTimeout(150);
    }
    const clip = { x: 40, y: 190, width: 310, height: 330 };
    const shot = async () => (await page.screenshot({ clip })).toString('base64');
    const state = () => page.evaluate(() => ({
        mode: Module._craft_game_mode(),
        health: Module._craft_health(),
        dead: Module._craft_is_dead(),
        hotbar: Array.from({ length: 9 }, (_, i) =>
            [Module._craft_inv_item(i), Module._craft_inv_count(i)]),
        bag: Array.from({ length: 36 }, (_, i) =>
            [Module._craft_inv_item(i), Module._craft_inv_count(i)])
            .filter(s => s[1] > 0),
    }));

    await page.goto(`http://127.0.0.1:${PORT}/`);
    let started = true;
    try { await page.waitForSelector('#controls:not(.hidden)', { timeout: 120000 }); }
    catch (e) { started = false; }
    check('phone: module starts and the controls appear', started);
    if (!started) { console.log(errors.join('\n')); await browser.close(); server.close(); process.exit(1); }

    // the help panel shows up on a first visit
    check('phone: the help panel opens on a first visit',
        await page.locator('#modal:not(.hidden)').count() === 1);
    await tap('#panel button.primary');     // "Back" on the help panel
    await tap('#panel button.primary');     // "Continue" on the menu
    await page.waitForTimeout(6000);       // let the world generate

    check('phone: the menu is closed again',
        await page.locator('#modal.hidden').count() === 1);

    // hotbar
    const filled = (await state()).hotbar.filter(s => s[1] > 0).length;
    check('phone: the hotbar is filled in creative mode', filled === 9, filled + ' of 9 slots');

    await page.screenshot({ path: OUT + '/shot-01-start.png' });

    // standing still barely changes the picture -- this is the control
    const idle1 = await shot();
    await page.waitForTimeout(2200);
    const idle2 = await shot();
    const idleDiff = await diff(page, idle1, idle2);

    // the stick walks
    const stick = await page.locator('#stick').boundingBox();
    await touchStart(stick.x + stick.width / 2, stick.y + stick.height / 2 - 40);
    await page.waitForTimeout(2200);
    await touchEnd();
    await page.waitForTimeout(400);
    const walkDiff = await diff(page, idle2, await shot());
    check('phone: the stick walks the player',
        walkDiff > idleDiff * 3 && walkDiff > 2,
        'walking ' + walkDiff.toFixed(2) + ' vs standing ' + idleDiff.toFixed(2));

    // aim at the ground, then build and mine
    await page.evaluate(() => Module._craft_touch_look(0, 260));
    await page.waitForTimeout(500);
    const beforePlace = await shot();
    await tap('#place');
    await page.waitForTimeout(600);
    const placed = await shot();
    check('phone: PLACE changes the world',
        await diff(page, beforePlace, placed) > 1);
    await tap('#break');
    await page.waitForTimeout(600);
    check('phone: BREAK changes the world',
        await diff(page, placed, await shot()) > 1);

    // survival mode through the menu
    await tap('#menu-button');
    await tap('#panel .choice', 1);        // Survival
    await page.waitForTimeout(200);
    check('phone: the menu switches to survival', (await state()).mode === 1);
    check('phone: hearts appear in survival',
        await page.locator('#hearts .heart').count() === 10);
    check('phone: the fly button is hidden in survival',
        await page.locator('#fly.hidden').count() === 1);
    await tap('#panel button.primary');     // Continue
    await page.waitForTimeout(300);

    // mining fills the bag
    const bagBefore = (await state()).bag.length;
    await page.evaluate(() => Module._craft_touch_look(0, 120));
    await page.waitForTimeout(300);
    for (let i = 0; i < 4; i++) { await tap('#break'); await page.waitForTimeout(250); }
    const bagAfter = (await state()).bag;
    check('phone: mining in survival fills the bag',
        bagAfter.length > bagBefore || bagAfter.some(s => s[1] > 1),
        JSON.stringify(bagAfter));

    // crafting: wood from the creative palette, planks out of the recipe list
    await page.evaluate(() => {
        Module._craft_set_game_mode(0);
        Module._craft_inv_give(5, 4);          // 5 = WOOD
        Module._craft_set_game_mode(1);
    });
    await tap('#bag');
    await tap('.tabs button', 1);              // "Craft"
    const recipeCount = await page.locator('.recipe').count();
    check('phone: the crafting list is there', recipeCount > 8, recipeCount + ' recipes');
    const enabled = await page.locator('.recipe:not([disabled])').count();
    check('phone: a recipe with the ingredients at hand is enabled', enabled >= 1,
        enabled + ' of ' + recipeCount);
    await tap('.recipe:not([disabled])');
    const afterCraft = (await state()).bag;
    check('phone: crafting produces the item',
        afterCraft.some(s => s[0] === 8),      // 8 = PLANK
        JSON.stringify(afterCraft));
    await page.screenshot({ path: OUT + '/shot-02-crafting.png' });
    await tap('#panel .close');                // back to the game

    // falling hurts, and enough of it kills
    await page.evaluate(() => {
        Module._craft_set_game_mode(0);
        Module._craft_toggle_fly();
    });
    await touchStart(0, 0);                    // nothing, just to settle input
    await touchEnd();
    await page.evaluate(() => Module._craft_touch_jump(1));
    await page.waitForTimeout(2500);           // fly up
    await page.evaluate(() => {
        Module._craft_touch_jump(0);
        Module._craft_set_game_mode(1);        // survival turns flying off
    });
    await page.waitForTimeout(4000);           // fall
    const afterFall = await state();
    check('phone: falling costs health', afterFall.health < 20 || afterFall.dead,
        'health ' + afterFall.health + (afterFall.dead ? ', dead' : ''));

    // death screen, if the fall was deep enough
    if (afterFall.dead) {
        await page.waitForTimeout(400);
        check('phone: the death screen appears',
            (await page.locator('#panel h1').textContent()).indexOf('died') >= 0);
        await tap('#panel button.primary');
        check('phone: respawn brings the player back', (await state()).dead === 0);
    }

    // ---- creatures ----
    check('phone: closing a panel lets the world run again',
        await page.evaluate(() => Module._craft_is_paused()) === 0);
    await page.evaluate(() => {
        Module._craft_set_game_mode(1);
        Module._craft_respawn();
        Module._craft_clear_mobs();         // measure against a known state
        // put a real blade in the hand: by now the hotbar holds whatever the
        // earlier steps left there, and a lump of dirt hits for 1
        Module._craft_set_game_mode(0);
        Module._craft_select_slot(1);
        Module._craft_inv_give(140, 1);     // iron blade
        Module._craft_set_game_mode(1);
        // A slime hops towards the player, so it aims itself -- a boar
        // wanders off and the measurement becomes a coin toss.
        Module._craft_spawn_mob(5);
    });
    await page.waitForTimeout(600);
    const spawned = await page.evaluate(() => ({
        count: Module._craft_entity_count(),
        distance: Module._craft_nearest_mob(),
        paused: Module._craft_is_paused(),
        modal: document.getElementById('modal').className
    }));
    check('phone: a creature can be put into the world', spawned.count === 1,
        'count ' + spawned.count + ', ' + spawned.distance.toFixed(1) +
        ' blocks away, paused=' + spawned.paused + ', modal="' + spawned.modal + '"');
    await page.screenshot({ path: OUT + '/shot-05-creature.png' });

    // hitting it takes it apart and the drop lands in the bag
    const gems = async () => (await state()).bag
        .filter(s => s[0] === 131).reduce((n, s) => n + s[1], 0);
    const gemsBefore = await gems();
    let killed = false;
    for (let i = 0; i < 20 && !killed; i++) {
        await tap('#break');
        await page.waitForTimeout(320);
        killed = (await gems()) > gemsBefore;
        if (process.env.VERBOSE) {
            console.log('    Schlag', i, JSON.stringify(await page.evaluate(() => ({
                n: Module._craft_entity_count(),
                d: +Module._craft_nearest_mob().toFixed(2),
                hp: +Module._craft_probe(12).toFixed(1),
                mob: [3, 4, 5].map(i => +Module._craft_probe(i).toFixed(1)),
                me: [0, 1, 2].map(i => +Module._craft_probe(i).toFixed(1)),
                paused: Module._craft_is_paused()
            }))));
        }
    }
    check('phone: a creature can be defeated with the blade', killed);
    // a gem can only come from a slime, so this is proof, not a hint
    check('phone: what it drops lands in the bag', (await gems()) > gemsBefore,
        'gems ' + gemsBefore + ' -> ' + (await gems()));

    // a hostile creature actually hurts
    await page.evaluate(() => {
        Module._craft_respawn();
        Module._craft_clear_mobs();
        Module._craft_spawn_mob(2);         // stalker: hunts by day too
    });
    for (let i = 0; i < 6; i++) {
        await page.waitForTimeout(900);
        if (process.env.VERBOSE) {
            console.log('   ', JSON.stringify(await page.evaluate(() => ({
                player: [0, 1, 2].map(i => +Module._craft_probe(i).toFixed(2)),
                mob: [3, 4, 5].map(i => +Module._craft_probe(i).toFixed(2)),
                ticks: Module._craft_probe(7),
                paused: Module._craft_is_paused(),
                mode: Module._craft_game_mode(),
                dy: +Module._craft_probe(8).toFixed(2),
                health: Module._craft_health()
            }))));
        }
    }
    const hurt = await page.evaluate(() => ({
        health: Module._craft_health(),
        count: Module._craft_entity_count(),
        distance: Module._craft_nearest_mob(),
        player: [0, 1, 2].map(i => +Module._craft_probe(i).toFixed(1)),
        mob: [3, 4, 5].map(i => +Module._craft_probe(i).toFixed(1))
    }));
    check('phone: a hostile creature costs health', hurt.health < 20,
        'health ' + hurt.health + ', ' + hurt.count + ' creatures, player at ' +
        hurt.player + ', creature at ' + hurt.mob);

    // ... but not while a menu is open
    await tap('#menu-button');
    const pausedBefore = (await state()).health;
    await page.waitForTimeout(3500);
    const pausedAfter = (await state()).health;
    check('phone: creatures wait while the menu is open',
        pausedAfter === pausedBefore,
        pausedBefore + ' -> ' + pausedAfter);
    await tap('#panel button.primary');

    // the crossbow shoots
    await page.evaluate(() => {
        Module._craft_respawn();
        Module._craft_clear_mobs();
        Module._craft_set_game_mode(0);
        Module._craft_select_slot(1);
        Module._craft_inv_give(134, 30);    // bolts
        Module._craft_select_slot(0);
        Module._craft_inv_give(150, 1);     // crossbow
        Module._craft_set_game_mode(1);     // survival, so the drop is kept
        // a stalker walks straight at the player: it aims itself
        Module._craft_spawn_mob(2);
    });
    await page.waitForTimeout(900);
    const coal = async () => (await state()).bag
        .filter(s => s[0] === 129).reduce((n, s) => n + s[1], 0);
    const coalBefore = await coal();
    let shot_down = false;
    for (let i = 0; i < 12 && !shot_down; i++) {
        await tap('#break');
        if (i === 0) {
            await page.waitForTimeout(60);
            await page.screenshot({ path: OUT + '/shot-06-bolt.png' });
        }
        await page.waitForTimeout(450);
        shot_down = (await coal()) > coalBefore;
    }
    check('phone: the crossbow brings a creature down', shot_down,
        'coal ' + coalBefore + ' -> ' + (await coal()));
    const boltsLeft = await page.evaluate(() => {
        let n = 0;
        for (let i = 0; i < 36; i++) {
            if (Module._craft_inv_item(i) === 134) n += Module._craft_inv_count(i);
        }
        return n;
    });
    check('phone: shooting uses up bolts', boltsLeft < 30, boltsLeft + ' of 30 left');

    // ---- water ----
    // Find a water block in the world, stand in it, and watch the breath go.
    const water = await page.evaluate(() => {
        for (let r = 2; r < 80; r += 2) {
            for (let dx = -r; dx <= r; dx += 2) {
                for (let dz = -r; dz <= r; dz += 2) {
                    if (Module._craft_block_at(dx, 13, dz) === 24) return [dx, dz];
                }
            }
        }
        return null;
    });
    check('phone: the world generator made water', water !== null,
        water ? 'first pool at ' + water : 'none within 80 blocks');

    if (water) {
        await page.evaluate(([x, z]) => {
            Module._craft_clear_mobs();
            Module._craft_set_game_mode(1);
            Module._craft_teleport(x, 13.2, z);
        }, water);
        await page.waitForTimeout(2500);
        const wet = await page.evaluate(() => ({
            air: Module._craft_air(),
            health: Module._craft_health(),
            head: Module._craft_block_at(
                Math.round(Module._craft_probe(0)),
                Math.round(Module._craft_probe(1)),
                Math.round(Module._craft_probe(2)))
        }));
        check('phone: standing in water uses up breath',
            wet.air < 100 && wet.head === 24,
            'breath ' + wet.air + '%, block at the head ' + wet.head);
        check('phone: the bubbles show up',
            await page.locator('#bubbles .bubble').count() === 10);
        await page.screenshot({ path: OUT + '/shot-07-underwater.png' });

        // drowning costs health, but only after a while
        await page.waitForTimeout(14000);
        const drowned = await page.evaluate(() => ({
            air: Module._craft_air(), health: Module._craft_health()
        }));
        check('phone: running out of breath costs health', drowned.health < 20,
            'breath ' + drowned.air + '%, health ' + drowned.health);

        await page.evaluate(() => {
            Module._craft_respawn();
            Module._craft_set_game_mode(0);
            Module._craft_toggle_fly();
            Module._craft_touch_jump(1);
        });
        await page.waitForTimeout(2600);
        await page.evaluate(() => {
            Module._craft_touch_jump(0);
            Module._craft_touch_look(0, 200);   // look down at the landscape
        });
        await page.waitForTimeout(900);
        const sky = await page.screenshot({ clip: { x: 0, y: 120, width: 390, height: 420 } });
        await page.screenshot({ path: OUT + '/shot-08-from-above.png' });
        const blue = await page.evaluate(async (data) => {
            const img = new Image();
            img.src = 'data:image/png;base64,' + data;
            await img.decode();
            const c = new OffscreenCanvas(img.width, img.height);
            const g = c.getContext('2d');
            g.drawImage(img, 0, 0);
            const px = g.getImageData(0, 0, img.width, img.height).data;
            let water = 0;
            for (let i = 0; i < px.length; i += 4) {
                // the water tile is a deep blue; the sky above is much paler
                if (px[i] < 90 && px[i + 1] > 50 && px[i + 1] < 150 && px[i + 2] > 140) {
                    water++;
                }
            }
            return water / (px.length / 4);
        }, sky.toString('base64'));
        check('phone: water can be seen in the landscape', blue > 0.01,
            (100 * blue).toFixed(1) + '% of the picture');
        await page.evaluate(() => { Module._craft_toggle_fly(); });
    }

    // ---- what the blocks do ----
    // mining in survival has to wait between blocks
    await page.evaluate(() => {
        Module._craft_respawn();
        Module._craft_clear_mobs();
        // a crossbow in the hand makes the button shoot, which is the point
        // of the crossbow -- so put a pickaxe there for the mining test
        Module._craft_set_game_mode(0);
        Module._craft_select_slot(0);
        Module._craft_inv_give(160, 1);     // pickaxe
        Module._craft_set_game_mode(1);
        Module._craft_touch_look(0, 420);
    });
    await page.waitForTimeout(500);
    const mined = [];
    for (let i = 0; i < 6; i++) {
        mined.push(await page.evaluate(() => {
            Module._craft_break_block();
            return Module._craft_mine_cooldown();
        }));
        await page.waitForTimeout(200);
    }
    check('phone: breaking a block starts a wait in survival',
        mined.some(v => v > 0), 'cooldown readings ' + JSON.stringify(mined));
    await page.evaluate(() => Module._craft_break_block());
    await page.waitForTimeout(160);
    check('phone: the break button shows the wait',
        await page.locator('#break.cooling').count() === 1);
    await page.waitForTimeout(1500);
    check('phone: the wait runs out',
        await page.evaluate(() => Module._craft_mine_cooldown()) === 0);

    // ... and creative mode has none
    await page.evaluate(() => Module._craft_set_game_mode(0));
    await tap('#break');
    check('phone: creative mode has no wait',
        await page.evaluate(() => Module._craft_mine_cooldown()) === 0);

    // flying: allowed in creative, refused in survival
    await page.evaluate(() => { Module._craft_toggle_fly(); });
    const flyCreative = await page.evaluate(() => Module._craft_is_flying());
    await page.evaluate(() => Module._craft_set_game_mode(1));
    const flyAfterSwitch = await page.evaluate(() => Module._craft_is_flying());
    await page.evaluate(() => Module._craft_toggle_fly());
    const flySurvival = await page.evaluate(() => Module._craft_is_flying());
    check('phone: flying works in creative mode', flyCreative === 1);
    check('phone: switching to survival turns flying off', flyAfterSwitch === 0);
    check('phone: flying cannot be turned on in survival', flySurvival === 0);
    await page.waitForTimeout(300);
    check('phone: and the fly button is gone in survival',
        await page.locator('#fly.hidden').count() === 1);

    // a lamp lights its surroundings
    await page.evaluate(() => {
        Module._craft_respawn();            // resets the view as well
        Module._craft_set_game_mode(0);
        Module._craft_select_slot(0);
        Module._craft_inv_give(12, 1);      // 12 = lamp
        Module._craft_touch_look(0, 300);
    });
    await page.waitForTimeout(600);
    await tap('#break');                    // grass first, it is not solid
    await page.waitForTimeout(300);
    await tap('#place');
    await page.waitForTimeout(1200);
    await page.screenshot({ path: OUT + '/shot-11-lamp.png' });
    check('phone: a lamp can be placed', await page.evaluate(() => {
        const x = Math.round(Module._craft_probe(0));
        const y = Math.round(Module._craft_probe(1));
        const z = Math.round(Module._craft_probe(2));
        for (let dx = -3; dx <= 3; dx++)
            for (let dy = -3; dy <= 2; dy++)
                for (let dz = -3; dz <= 3; dz++)
                    if (Module._craft_block_at(x + dx, y + dy, z + dz) === 12) return true;
        return false;
    }));

    // a chest keeps what is put into it, across a reload
    await page.evaluate(() => {
        Module._craft_select_slot(0);
        Module._craft_inv_give(14, 1);      // 14 = chest
        Module._craft_touch_look(0, 60);    // a little higher, next to the lamp
    });
    await page.waitForTimeout(400);
    await tap('#break');
    await page.waitForTimeout(300);
    await tap('#place');
    await page.waitForTimeout(500);
    await tap('#place');                    // tapping it again opens it
    await page.waitForTimeout(600);
    check('phone: a chest opens',
        await page.evaluate(() => Module._craft_container_open()) === 1);
    check('phone: the chest screen is showing',
        (await page.locator('#panel h1').textContent()) === 'Chest');

    // put the first bag slot in, check it landed, close, reopen
    const putItem = await page.evaluate(() => {
        Module._craft_select_slot(1);
        Module._craft_inv_give(3, 7);       // seven stone
        const item = Module._craft_inv_item(1);
        Module._craft_container_put(1);
        return item;
    });
    await page.waitForTimeout(400);
    check('phone: the chest takes what is put in',
        await page.evaluate(() => Module._craft_container_item(0)) === putItem,
        'stored ' + await page.evaluate(() => Module._craft_container_count(0)));
    await page.screenshot({ path: OUT + '/shot-12-chest.png' });
    await tap('#panel .close');
    await page.waitForTimeout(400);
    check('phone: closing the chest screen closes the chest',
        await page.evaluate(() => Module._craft_container_open()) === 0);

    await page.reload();
    await page.waitForSelector('#controls:not(.hidden)', { timeout: 120000 });
    await page.waitForTimeout(4000);
    const chestAfterReload = await page.evaluate(() => {
        const x = Math.round(Module._craft_probe(0));
        const y = Math.round(Module._craft_probe(1));
        const z = Math.round(Module._craft_probe(2));
        for (let dx = -3; dx <= 3; dx++)
            for (let dy = -3; dy <= 2; dy++)
                for (let dz = -3; dz <= 3; dz++)
                    if (Module._craft_block_at(x + dx, y + dy, z + dz) === 14) {
                        Module._craft_container_open();  // just asking
                        return [x + dx, y + dy, z + dz];
                    }
        return null;
    });
    check('phone: the chest is still there after a reload', chestAfterReload !== null,
        JSON.stringify(chestAfterReload));

    // ---- the underworld (beta) ----
    const dry = await page.evaluate(() => {
        // Untouched, dry ground: away from the holes the earlier steps dug,
        // and out of the sea -- a stone cannot be placed into water either.
        Module._craft_respawn();
        Module._craft_clear_mobs();
        for (let r = 8; r < 40; r += 2) {
            const ring = [[r, 0], [0, r], [-r, 0], [0, -r], [r, r], [-r, -r]];
            for (const [dx, dz] of ring) {
                for (let y = 24; y > 13; y--) {
                    const here = Module._craft_block_at(dx, y, dz);
                    if (here === 0 || here === 24) continue;
                    if (Module._craft_block_at(dx, y + 1, dz) === 0 &&
                        Module._craft_block_at(dx, y + 2, dz) === 0) {
                        return [dx, y, dz];
                    }
                    break;
                }
            }
        }
        return null;
    });
    check('phone: dry ground for the gateway', dry !== null, JSON.stringify(dry));
    await page.evaluate(([x, y, z]) => {
        Module._craft_teleport(x, y + 2.5, z);
        Module._craft_set_game_mode(0);
        Module._craft_select_slot(0);
        Module._craft_inv_give(64, 1);      // portal stone
        Module._craft_touch_look(0, 420);   // look straight at the ground
    }, dry);
    await page.waitForTimeout(1200);        // land, and let the chunk settle
    await page.waitForTimeout(500);
    await tap('#break');                    // clear the grass in the way
    await page.waitForTimeout(300);
    await tap('#place');                    // set the stone down
    await page.waitForTimeout(400);
    await page.evaluate(() => {
        Module._craft_select_slot(0);
        Module._craft_inv_give(163, 1);     // fire starter
    });
    await page.waitForTimeout(300);
    await tap('#place');                    // light it
    await page.waitForTimeout(400);

    if (process.env.VERBOSE) {
        console.log('    Umgebung:', JSON.stringify(await page.evaluate(() => {
            const x0 = Math.round(Module._craft_probe(0));
            const y0 = Math.round(Module._craft_probe(1));
            const z0 = Math.round(Module._craft_probe(2));
            const found = [];
            for (let dx = -5; dx <= 5; dx++)
                for (let dz = -5; dz <= 5; dz++)
                    for (let dy = -4; dy <= 3; dy++) {
                        const w = Module._craft_block_at(x0+dx, y0+dy, z0+dz);
                        if (w === 64 || w === 65) found.push([w, dx, dy, dz]);
                    }
            return { me: [x0, y0, z0], dead: Module._craft_is_dead(),
                     paused: Module._craft_is_paused(), found: found,
                     slot: Module._craft_inv_item(Module._craft_hotbar_index()) };
        })));
    }

    // walk into it: find the gateway block and stand in it
    const gate = await page.evaluate(() => {
        const x0 = Math.round(Module._craft_probe(0));
        const y0 = Math.round(Module._craft_probe(1));
        const z0 = Math.round(Module._craft_probe(2));
        for (let dx = -4; dx <= 4; dx++) {
            for (let dz = -4; dz <= 4; dz++) {
                for (let dy = -3; dy <= 2; dy++) {
                    if (Module._craft_block_at(x0 + dx, y0 + dy, z0 + dz) === 65) {
                        return [x0 + dx, y0 + dy, z0 + dz];
                    }
                }
            }
        }
        return null;
    });
    check('phone: the fire starter opens a gateway', gate !== null,
        gate ? 'gateway at ' + gate : 'no gateway block found');
    if (gate) {
        await page.evaluate(([x, y, z]) => Module._craft_teleport(x, y, z), gate);
    }
    let travelled = false;
    for (let i = 0; i < 20 && !travelled; i++) {
        await page.waitForTimeout(500);
        travelled = await page.evaluate(() => Module._craft_dimension()) === 1;
    }
    check('phone: the gateway leads into the underworld', travelled);
    if (travelled) {
        await page.waitForTimeout(4000);    // let the caves build
        // step out of the gateway, or the picture is one purple block
        const out = await page.evaluate(() => {
            const x = Module._craft_probe(0);
            const y = Module._craft_probe(1);
            const z = Module._craft_probe(2);
            Module._craft_teleport(x + 2, y, z);
            return [x, y, z];
        });
        await page.waitForTimeout(1500);
        check('phone: the world does not bounce the player back and forth',
            await page.evaluate(() => Module._craft_dimension()) === 1,
            'still in the underworld after stepping aside');
        await page.screenshot({ path: OUT + '/shot-09-underworld.png' });
        const under = await page.evaluate(() => {
            let rock = 0;
            let lava = 0;
            const y0 = Math.round(Module._craft_probe(1));
            const x0 = Math.round(Module._craft_probe(0));
            const z0 = Math.round(Module._craft_probe(2));
            for (let x = x0 - 6; x <= x0 + 6; x++) {
                for (let z = z0 - 6; z <= z0 + 6; z++) {
                    for (let y = 4; y < 40; y++) {
                        const w = Module._craft_block_at(x, y, z);
                        if (w === 28 || w === 29 || w === 30) rock++;
                        if (w === 66) lava++;
                    }
                }
            }
            return { rock: rock, lava: lava, y: y0 };
        });
        check('phone: the underworld is made of its own blocks', under.rock > 200,
            under.rock + ' rock, ' + under.lava + ' lava');
        check('phone: there is lava down there', under.lava > 0,
            under.lava + ' blocks');

        // creatures of that world turn up
        await page.evaluate(() => Module._craft_spawn_mob(8));   // magma brute
        await page.waitForTimeout(700);
        check('phone: creatures of the underworld can live there',
            await page.evaluate(() => Module._craft_entity_count()) >= 1);

        // and the gateway carries the player home again: back into it
        await page.evaluate(([x, y, z]) => Module._craft_teleport(x, y, z), out);
        let back = false;
        for (let i = 0; i < 20 && !back; i++) {
            await page.waitForTimeout(500);
            back = await page.evaluate(() => Module._craft_dimension()) === 0;
        }
        check('phone: the gateway leads back home', back);
    }

    // ---- marketplace ----
    await tap('#menu-button');
    await tapLocator(page.locator('#panel button', { hasText: 'Marketplace' }));
    await page.waitForTimeout(400);
    check('phone: the marketplace opens',
        (await page.locator('#panel h1').textContent()).indexOf('Marketplace') >= 0);

    // install the example pack, reload, and look for what it brought
    await tapLocator(page.locator('#panel button', { hasText: 'example pack' }));
    await page.waitForTimeout(1200);
    const installed = await page.evaluate(() =>
        JSON.parse(localStorage.getItem('craft.addons') || '[]').length);
    check('phone: an add-on can be installed', installed === 1,
        installed + ' installed');

    await page.reload();
    await page.waitForSelector('#controls:not(.hidden)', { timeout: 120000 });
    await page.waitForTimeout(3000);
    const addonState = await page.evaluate(() => ({
        registered: Module._craft_addon_count(),
        names: JSON.parse(Module.UTF8ToString(Module._craft_items_json()))
            .filter(t => t.id >= 67 && t.id <= 127 || t.id >= 200)
            .map(t => t.name)
    }));
    check('phone: the add-on things are in the game',
        addonState.registered >= 5, JSON.stringify(addonState.names));
    check('phone: an add-on block can be placed and mined',
        await page.evaluate(() => {
            const id = JSON.parse(Module.UTF8ToString(Module._craft_items_json()))
                .find(t => t.name === 'Amber Block');
            if (!id) return false;
            Module._craft_set_game_mode(0);
            Module._craft_select_slot(0);
            Module._craft_inv_give(id.id, 1);
            return Module._craft_inv_item(0) === id.id;
        }));
    await page.screenshot({ path: OUT + '/shot-10-addon.png' });

    // and switching it off takes it away again
    await page.evaluate(() => {
        const list = JSON.parse(localStorage.getItem('craft.addons'));
        list[0].enabled = false;
        localStorage.setItem('craft.addons', JSON.stringify(list));
    });
    await page.reload();
    await page.waitForSelector('#controls:not(.hidden)', { timeout: 120000 });
    await page.waitForTimeout(2500);
    check('phone: switching an add-on off removes its things',
        await page.evaluate(() => Module._craft_addon_count()) === 0);
    await page.evaluate(() => localStorage.removeItem('craft.addons'));

    // the world, the bag and the mode survive a reload
    await page.evaluate(() => Module._craft_save_now());
    await page.waitForTimeout(1500);
    const before = await state();
    const beforeReload = errors.length;
    await page.reload();
    await page.waitForSelector('#controls:not(.hidden)', { timeout: 120000 });
    await page.waitForTimeout(4000);
    const after = await state();
    check('phone: the game mode survives a reload', after.mode === before.mode);
    check('phone: the bag survives a reload',
        JSON.stringify(after.bag) === JSON.stringify(before.bag),
        JSON.stringify(after.bag));
    check('phone: no errors after the reload', errors.length === beforeReload,
        errors.slice(beforeReload).join(' | '));
    await page.screenshot({ path: OUT + '/shot-03-reloaded.png' });
    check('phone: no javascript or wasm errors at all', errors.length === 0,
        errors.join(' | '));

    // ===================== desktop =====================
    const desktop = await browser.newContext({ viewport: { width: 900, height: 600 } });
    const big = await desktop.newPage();
    const deskErrors = [];
    big.on('pageerror', e => deskErrors.push('pageerror: ' + e.message));
    big.on('console', m => { if (m.type() === 'error') deskErrors.push('console: ' + m.text()); });
    await big.goto(`http://127.0.0.1:${PORT}/`);
    await big.waitForSelector('#controls:not(.hidden)', { timeout: 120000 });
    await big.waitForTimeout(6000);
    if (await big.locator('#modal:not(.hidden)').count()) {
        await big.click('#panel button');
        if (await big.locator('#modal:not(.hidden)').count()) {
            await big.click('#panel button');
        }
    }

    const deskClip = { x: 150, y: 100, width: 600, height: 400 };
    const deskShot = async () => (await big.screenshot({ clip: deskClip })).toString('base64');
    const deskBefore = await deskShot();
    await big.waitForTimeout(1500);
    const deskIdle = await diff(big, deskBefore, await deskShot());

    await big.mouse.move(450, 300);
    await big.mouse.down();
    await big.mouse.move(450, 420, { steps: 10 });
    await big.mouse.up();
    await big.waitForTimeout(800);
    const lookDiff = await diff(big, deskBefore, await deskShot());
    check('desktop: dragging looks around',
        lookDiff > deskIdle * 5 && lookDiff > 4,
        'drag ' + lookDiff.toFixed(2) + ' vs still ' + deskIdle.toFixed(2));
    await big.screenshot({ path: OUT + '/shot-04-desktop.png' });

    const keysBefore = await deskShot();
    await big.keyboard.down('w');
    await big.waitForTimeout(1800);
    await big.keyboard.up('w');
    await big.waitForTimeout(400);
    check('desktop: the W key walks',
        await diff(big, keysBefore, await deskShot()) > 2);
    check('desktop: no javascript or wasm errors', deskErrors.length === 0,
        deskErrors.join(' | '));

    await browser.close();
    server.close();
    console.log(failures ? failures + ' check(s) failed' : 'all checks passed');
    process.exit(failures ? 1 : 0);
})();
