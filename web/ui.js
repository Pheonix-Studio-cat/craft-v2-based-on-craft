/* The browser interface of Craft: touch controls, hotbar, inventory,
 * crafting and the menu.
 *
 * Everything in here only ever *asks* the game: the rules live in C
 * (src/inventory.c, src/main.c), this file draws them and forwards taps.
 *
 * Two lessons from the port are baked in and should not be undone:
 *   - no setPointerCapture: it throws in some browsers and kills the controls
 *   - pointermove/pointerup are listened for on window, so a finger that
 *     slides off a button still ends its press
 */
var CraftUI = (function () {
    'use strict';

    var canvas, controls, modal, panel, loading, statusLine, bar, hotbarEl, heartsEl;
    var items = {};          // id -> {id, name, kind, block, tile, stack, damage, heal}
    var recipes = [];
    var atlas = new Image();
    var atlasReady = false;
    var iconCache = {};
    var started = false;     // the game said hello, see gameReady()
    var ready = false;       // the wasm runtime answers only after it is
    var lastVersion = -1;
    var lastHealth = -1;
    var pickedSlot = -1;
    var currentTab = 'bag';
    var modalKind = null;
    var sensitivity = 2.2;

    // ----------------------------------------------------------------- glue
    function call(name) {
        // Before the runtime is up, Module._craft_* exists but the wasm
        // exports behind it do not -- asking anyway throws.
        if (!ready) return 0;
        var fn = Module['_' + name];
        if (!fn) return 0;
        return fn.apply(null, Array.prototype.slice.call(arguments, 1));
    }

    function text(name) {
        var pointer = call(name);
        return pointer ? Module.UTF8ToString(pointer) : '';
    }

    function el(tag, className, content) {
        var node = document.createElement(tag);
        if (className) node.className = className;
        if (content !== undefined) node.textContent = content;
        return node;
    }

    // ---------------------------------------------------------------- icons
    // The atlas is the game's own texture file. Tile rows count from the
    // bottom (the game flips the image when it loads it), and the colour the
    // block shader discards -- magenta -- becomes transparent here.
    function icon(tile) {
        if (iconCache[tile]) return iconCache[tile];
        if (!atlasReady || !tile) return '';
        var c = document.createElement('canvas');
        c.width = c.height = 16;
        var ctx = c.getContext('2d');
        var col = tile % 16;
        var row = Math.floor(tile / 16);
        ctx.drawImage(atlas, col * 16, atlas.height - (row + 1) * 16, 16, 16, 0, 0, 16, 16);
        try {
            var data = ctx.getImageData(0, 0, 16, 16);
            for (var i = 0; i < data.data.length; i += 4) {
                if (data.data[i] > 230 && data.data[i + 1] < 40 && data.data[i + 2] > 230) {
                    data.data[i + 3] = 0;
                }
            }
            ctx.putImageData(data, 0, 0);
        } catch (e) {
            // a tainted canvas would only cost the transparency, not the icon
        }
        var url = 'url(' + c.toDataURL() + ')';
        iconCache[tile] = url;
        return url;
    }

    function paintSlot(node, item, count) {
        var type = items[item];
        node.style.backgroundImage = type ? icon(type.tile) : '';
        node.innerHTML = '';
        if (count > 1) {
            node.appendChild(el('span', 'count', String(count)));
        }
        node.title = type ? type.name : '';
    }

    // --------------------------------------------------------------- hotbar
    function buildHotbar() {
        hotbarEl.innerHTML = '';
        for (var i = 0; i < 9; i++) {
            var slot = el('div', 'slot');
            slot.dataset.index = String(i);
            slot.addEventListener('pointerdown', function (event) {
                call('craft_select_slot', parseInt(event.currentTarget.dataset.index, 10));
                renderHotbar();
                event.preventDefault();
                event.stopPropagation();
            });
            hotbarEl.appendChild(slot);
        }
    }

    function renderHotbar() {
        var selected = call('craft_hotbar_index');
        for (var i = 0; i < hotbarEl.children.length; i++) {
            var node = hotbarEl.children[i];
            paintSlot(node, call('craft_inv_item', i), call('craft_inv_count', i));
            node.classList.toggle('selected', i === selected);
        }
    }

    function renderHearts() {
        var survival = call('craft_game_mode') === 1;
        heartsEl.classList.toggle('hidden', !survival);
        if (!survival) return;
        var health = call('craft_health');
        var max = call('craft_max_health');
        heartsEl.innerHTML = '';
        for (var i = 0; i < max / 2; i++) {
            var full = health >= (i + 1) * 2;
            var half = !full && health === i * 2 + 1;
            heartsEl.appendChild(el('div', 'heart' + (full ? ' full' : half ? ' half' : '')));
        }
    }

    // ---------------------------------------------------------------- modal
    function openModal(kind) {
        modalKind = kind;
        // let go of the controls, or the player walks on behind the menu
        call('craft_touch_move', 0, 0, 0);
        call('craft_touch_jump', 0);
        stickRelease();
        modal.classList.remove('hidden');
        renderModal();
    }

    function closeModal() {
        modalKind = null;
        pickedSlot = -1;
        modal.classList.add('hidden');
        call('craft_save_now');
    }

    function renderModal() {
        if (!modalKind) return;
        panel.innerHTML = '';
        if (modalKind === 'menu') renderMenu();
        else if (modalKind === 'inventory') renderInventory();
        else if (modalKind === 'death') renderDeath();
        else if (modalKind === 'help') renderHelp();
    }

    function button(label, hint, className, onClick) {
        var node = el('button', 'wide' + (className ? ' ' + className : ''));
        node.appendChild(document.createTextNode(label));
        if (hint) node.appendChild(el('small', null, hint));
        node.addEventListener('click', onClick);
        return node;
    }

    // ----------------------------------------------------------------- menu
    function renderMenu() {
        panel.appendChild(el('h1', null, 'Craft'));
        panel.appendChild(el('p', null, 'Mine, build, survive — in a browser tab.'));

        panel.appendChild(button('▶  Continue', null, 'primary', closeModal));

        panel.appendChild(el('h2', null, 'Game mode'));
        var row = el('div', 'row');
        [['Creative', 'Fly, unlimited blocks, nothing can hurt you.', 0],
         ['Survival', 'Health, falling hurts, blocks have to be mined.', 1]
        ].forEach(function (entry) {
            var node = el('button', 'choice');
            node.appendChild(document.createTextNode(entry[0]));
            node.appendChild(el('small', null, entry[1]));
            if (call('craft_game_mode') === entry[2]) node.classList.add('active');
            node.addEventListener('click', function () {
                call('craft_set_game_mode', entry[2]);
                renderModal();
                renderHearts();
                renderHotbar();
                updateButtons();
            });
            row.appendChild(node);
        });
        panel.appendChild(row);

        panel.appendChild(el('h2', null, 'More'));
        panel.appendChild(button('🎒  Inventory and crafting', null, null, function () {
            openModal('inventory');
        }));
        panel.appendChild(button('?  How to play', null, null, function () {
            openModal('help');
        }));

        panel.appendChild(el('h2', null, 'Look sensitivity'));
        var slider = el('input');
        slider.type = 'range';
        slider.min = '0.5';
        slider.max = '5';
        slider.step = '0.1';
        slider.value = String(sensitivity);
        slider.style.width = '100%';
        slider.addEventListener('input', function () {
            sensitivity = parseFloat(slider.value);
            try { localStorage.setItem('craft.sensitivity', String(sensitivity)); } catch (e) {}
        });
        panel.appendChild(slider);

        panel.appendChild(button('🗑  Start a new world', 'Deletes everything built so far.',
            'danger', confirmReset));
    }

    function confirmReset() {
        panel.innerHTML = '';
        panel.appendChild(el('h1', null, 'Start a new world?'));
        panel.appendChild(el('p', null,
            'Every block placed, the inventory and the position are deleted. ' +
            'This cannot be undone.'));
        panel.appendChild(button('Yes, delete it', null, 'danger', resetWorld));
        panel.appendChild(button('No, keep playing', null, null, function () {
            openModal('menu');
        }));
    }

    function resetWorld() {
        panel.innerHTML = '';
        panel.appendChild(el('h1', null, 'Deleting…'));
        try { Module.FS.unlink('/craft/craft.db'); } catch (e) {}
        try { Module.FS.unlink('/craft/auth.db'); } catch (e) {}
        Module.FS.syncfs(false, function () { location.reload(); });
    }

    // ------------------------------------------------------------ inventory
    function renderInventory() {
        panel.appendChild(el('h1', null, 'Inventory'));

        var tabs = el('div', 'tabs');
        var names = [['bag', 'Bag'], ['craft', 'Craft']];
        if (call('craft_game_mode') === 0) names.push(['blocks', 'All blocks']);
        names.forEach(function (entry) {
            var node = el('button', currentTab === entry[0] ? 'active' : '', entry[1]);
            node.addEventListener('click', function () {
                currentTab = entry[0];
                pickedSlot = -1;
                renderModal();
            });
            tabs.appendChild(node);
        });
        panel.appendChild(tabs);

        if (currentTab === 'bag') renderBag();
        else if (currentTab === 'craft') renderCrafting();
        else renderBlockPalette();

        panel.appendChild(button('▶  Back to the game', null, 'primary', closeModal));
    }

    function slotNode(index) {
        var node = el('div', 'slot');
        paintSlot(node, call('craft_inv_item', index), call('craft_inv_count', index));
        if (index === pickedSlot) node.classList.add('picked');
        node.addEventListener('click', function () {
            if (pickedSlot < 0) {
                if (call('craft_inv_count', index) > 0) pickedSlot = index;
            }
            else if (pickedSlot === index) {
                pickedSlot = -1;
            }
            else {
                call('craft_inv_move', pickedSlot, index);
                pickedSlot = -1;
            }
            renderModal();
            renderHotbar();
        });
        return node;
    }

    function renderBag() {
        panel.appendChild(el('p', null, pickedSlot < 0
            ? 'Tap an item, then tap where it should go.'
            : 'Now tap the slot it should move to.'));
        var grid = el('div', 'grid');
        for (var i = 9; i < 36; i++) grid.appendChild(slotNode(i));
        panel.appendChild(grid);

        var hotbar = el('div', 'grid hotbar-row');
        for (var j = 0; j < 9; j++) hotbar.appendChild(slotNode(j));
        panel.appendChild(hotbar);

        if (pickedSlot >= 0) {
            panel.appendChild(button('🗑  Throw this stack away', null, 'danger', function () {
                call('craft_inv_drop', pickedSlot);
                pickedSlot = -1;
                renderModal();
                renderHotbar();
            }));
        }
    }

    function renderCrafting() {
        panel.appendChild(el('p', null,
            'No workbench needed. Grey means something is missing.'));
        recipes.forEach(function (recipe, index) {
            var type = items[recipe.result];
            if (!type) return;
            var node = el('button', 'recipe');
            var pic = el('div', 'icon');
            pic.style.backgroundImage = icon(type.tile);
            node.appendChild(pic);
            var name = el('div', 'name');
            name.appendChild(document.createTextNode(
                type.name + (recipe.count > 1 ? ' ×' + recipe.count : '')));
            name.appendChild(el('div', 'need', recipe.need.map(function (pair) {
                var ing = items[pair[0]];
                return (ing ? ing.name : '?') + ' ×' + pair[1];
            }).join(' · ')));
            node.appendChild(name);
            if (!call('craft_can_craft', index)) node.disabled = true;
            node.addEventListener('click', function () {
                call('craft_do_craft', index);
                renderModal();
                renderHotbar();
            });
            panel.appendChild(node);
        });
    }

    function renderBlockPalette() {
        panel.appendChild(el('p', null,
            'Creative mode: tap a block to put it in the selected hotbar slot.'));
        var grid = el('div', 'grid');
        Object.keys(items).forEach(function (key) {
            var type = items[key];
            if (!type.block) return;
            var node = el('div', 'slot');
            node.style.backgroundImage = icon(type.tile);
            node.title = type.name;
            node.addEventListener('click', function () {
                call('craft_inv_give', type.id, 1);
                renderHotbar();
                renderModal();
            });
            grid.appendChild(node);
        });
        panel.appendChild(grid);
    }

    // ---------------------------------------------------------------- death
    function renderDeath() {
        panel.appendChild(el('h1', null, 'You died'));
        panel.appendChild(el('p', null,
            'Your things stay with you — only the way back is long.'));
        panel.appendChild(button('↺  Respawn', null, 'primary', function () {
            call('craft_respawn');
            closeModal();
        }));
    }

    function renderHelp() {
        panel.appendChild(el('h1', null, 'How to play'));
        var list = el('ul');
        [['Look', 'drag anywhere on the world'],
         ['Walk', 'the stick at the bottom left'],
         ['⛏ / ▣', 'break and place the block in the crosshair'],
         ['🎒', 'inventory and crafting'],
         ['✈', 'flying, creative mode only'],
         ['Keyboard', 'WASD, space, 1–9, tab']
        ].forEach(function (entry) {
            var item = el('li');
            item.appendChild(el('b', null, entry[0]));
            item.appendChild(document.createTextNode(' — ' + entry[1]));
            list.appendChild(item);
        });
        panel.appendChild(list);
        panel.appendChild(button('▶  Back', null, 'primary', function () {
            openModal('menu');
        }));
    }

    // ------------------------------------------------------------- controls
    var lookId = null, lookX = 0, lookY = 0;
    var stickId = null, stick, knob, radius = 40;

    function stickTo(event) {
        var box = stick.getBoundingClientRect();
        var dx = event.clientX - (box.left + box.width / 2);
        var dy = event.clientY - (box.top + box.height / 2);
        var length = Math.hypot(dx, dy);
        var scale = length > radius ? radius / length : 1;
        knob.style.transform = 'translate(' + (dx * scale) + 'px,' + (dy * scale) + 'px)';
        call('craft_touch_move', dx / radius, dy / radius, Math.min(length / radius, 1));
    }

    function stickRelease() {
        stickId = null;
        if (knob) knob.style.transform = '';
        call('craft_touch_move', 0, 0, 0);
    }

    function hold(element, onDown, onUp) {
        var pointer = null;
        var repeat = null;
        function up(event) {
            if (pointer === null || (event && event.pointerId !== pointer)) return;
            pointer = null;
            clearTimeout(repeat);
            repeat = null;
            element.classList.remove('down');
            if (onUp) onUp();
        }
        element.addEventListener('pointerdown', function (event) {
            pointer = event.pointerId;
            element.classList.add('down');
            onDown();
            if (element.dataset.repeat) {
                repeat = setTimeout(function again() {
                    onDown();
                    repeat = setTimeout(again, 240);
                }, 420);
            }
            event.preventDefault();
            event.stopPropagation();
        });
        window.addEventListener('pointerup', up);
        window.addEventListener('pointercancel', up);
    }

    function setupControls() {
        stick = document.getElementById('stick');
        knob = document.getElementById('knob');

        canvas.addEventListener('pointerdown', function (event) {
            if (lookId !== null) return;
            lookId = event.pointerId;
            lookX = event.clientX;
            lookY = event.clientY;
        });
        stick.addEventListener('pointerdown', function (event) {
            stickId = event.pointerId;
            stickTo(event);
            event.preventDefault();
        });
        window.addEventListener('pointermove', function (event) {
            if (event.pointerId === stickId) {
                stickTo(event);
            }
            else if (event.pointerId === lookId) {
                call('craft_touch_look',
                    (event.clientX - lookX) * sensitivity,
                    (event.clientY - lookY) * sensitivity);
                lookX = event.clientX;
                lookY = event.clientY;
            }
        });
        function release(event) {
            if (event.pointerId === stickId) stickRelease();
            if (event.pointerId === lookId) lookId = null;
        }
        window.addEventListener('pointerup', release);
        window.addEventListener('pointercancel', release);

        hold(document.getElementById('break'), function () { call('craft_break_block'); });
        hold(document.getElementById('place'), function () { call('craft_place_block'); });
        hold(document.getElementById('jump'),
            function () { call('craft_touch_jump', 1); },
            function () { call('craft_touch_jump', 0); });

        var fly = document.getElementById('fly');
        hold(fly, function () {
            call('craft_toggle_fly');
            fly.classList.toggle('on', !!call('craft_is_flying'));
        });
        document.getElementById('bag').addEventListener('click', function () {
            currentTab = 'bag';
            openModal('inventory');
        });
        document.getElementById('menu-button').addEventListener('click', function () {
            openModal('menu');
        });
    }

    function updateButtons() {
        var creative = call('craft_game_mode') === 0;
        var fly = document.getElementById('fly');
        fly.classList.toggle('hidden', !creative);
        if (!creative) fly.classList.remove('on');
    }

    // ------------------------------------------------------------ main loop
    function poll() {
        var version = call('craft_inv_version');
        if (version !== lastVersion) {
            lastVersion = version;
            renderHotbar();
            if (modalKind === 'inventory') renderModal();
        }
        var health = call('craft_health');
        if (health !== lastHealth) {
            lastHealth = health;
            renderHearts();
        }
        if (call('craft_is_dead') && modalKind !== 'death') {
            openModal('death');
        }
    }

    function resize() {
        var ratio = Math.min(window.devicePixelRatio || 1, 2);
        call('craft_resize',
            Math.round(window.innerWidth * ratio),
            Math.round(window.innerHeight * ratio));
    }

    // Called by the game itself (src/main.c), once the item registry exists.
    function gameReady() {
        if (started) return;
        started = true;
        ready = true;
        try {
            sensitivity = parseFloat(localStorage.getItem('craft.sensitivity') || '2.2');
        } catch (e) {}
        JSON.parse(text('craft_items_json')).forEach(function (type) {
            items[type.id] = type;
        });
        recipes = JSON.parse(text('craft_recipes_json'));

        loading.classList.add('done');
        controls.classList.remove('hidden');
        buildHotbar();
        renderHotbar();
        renderHearts();
        updateButtons();
        resize();
        setInterval(poll, 200);

        var seen = false;
        try { seen = localStorage.getItem('craft.seen') === '1'; } catch (e) {}
        if (!seen) {
            openModal('help');
            try { localStorage.setItem('craft.seen', '1'); } catch (e) {}
        }
    }

    function moduleConfig() {
        canvas = document.getElementById('canvas');
        controls = document.getElementById('controls');
        modal = document.getElementById('modal');
        panel = document.getElementById('panel');
        loading = document.getElementById('loading');
        statusLine = document.getElementById('status');
        bar = document.querySelector('#bar > div');
        hotbarEl = document.getElementById('hotbar');
        heartsEl = document.getElementById('hearts');

        atlas.onload = function () {
            atlasReady = true;
            iconCache = {};
            if (ready) {
                renderHotbar();
                renderModal();
            }
        };
        atlas.src = 'texture.png';

        setupControls();
        window.addEventListener('resize', resize);
        window.addEventListener('orientationchange', function () {
            setTimeout(resize, 300);
        });
        window.addEventListener('error', function (event) {
            var banner = el('div', null, 'error: ' + event.message);
            banner.style.cssText = 'position:absolute;left:0;right:0;top:0;padding:8px 12px;' +
                'background:rgba(150,30,30,0.92);font-size:12px;pointer-events:auto;z-index:9';
            banner.addEventListener('click', function () { banner.remove(); });
            controls.appendChild(banner);
        });

        return {
            canvas: canvas,
            print: function (line) { console.log(line); },
            printErr: function (line) { console.error(line); },
            setStatus: function (line) {
                var match = /([^(]+)\((\d+(\.\d+)?)\/(\d+)\)/.exec(line || '');
                if (match) {
                    bar.style.width = (100 * (+match[2] / +match[4])) + '%';
                    statusLine.textContent = match[1].trim();
                }
                else if (line) {
                    statusLine.textContent = line;
                }
            },
            onRuntimeInitialized: function () {
                bar.style.width = '100%';
                // The interface waits for gameReady(); main() has not run yet.
            }
        };
    }

    return { moduleConfig: moduleConfig, gameReady: gameReady };
})();
