// ═══════════════════════════════════════════
// ESP32 MCPWM Driver — Web UI Controller
// ═══════════════════════════════════════════

'use strict';

/* ─── State ─── */
const state = {
    enable: false, frequency: 40, duty: 45,
    deadTimeRed: 200, deadTimeFed: 200,
    power: 0, voltage: 0, current: 0, temperature: 25, connected: false,
};

/* ─── DOM cache ─── */
const $ = id => document.getElementById(id);
const dom = {
    sysStatus: $('sysStatus'), statusDot: $('statusDot'), statusText: $('statusText'),
    wifiInfo: $('wifiInfo'), btnEStop: $('btnEStop'), chkEnable: $('chkEnable'),
    lblEnable: $('lblEnable'), powerRing: $('powerRing'), ringPowerVal: $('ringPowerVal'),
    metaVoltage: $('metaVoltage'), metaCurrent: $('metaCurrent'), metaTemp: $('metaTemp'),
    badgeFreq: $('badgeFreq'), sliderFreq: $('sliderFreq'), numFreq: $('numFreq'),
    badgeDuty: $('badgeDuty'), sliderDuty: $('sliderDuty'), numDuty: $('numDuty'),
    badgeRed: $('badgeRed'), sliderRed: $('sliderRed'), numRed: $('numRed'),
    badgeFed: $('badgeFed'), sliderFed: $('sliderFed'), numFed: $('numFed'),
    ledA: $('ledA'), ledB: $('ledB'), waveSvg: $('waveSvg'),
};

/* ═══════════════════════════════════════════
 *  Slider <-> Number two-way binding
 * ═══════════════════════════════════════════ */

function bindSliderNum(sliderId, numId, badgeId, fmt) {
    const sl = $(sliderId), nu = $(numId), bd = $(badgeId);
    sl.addEventListener('input', () => {
        const v = parseFloat(sl.value); nu.value = v;
        if (bd) bd.textContent = fmt(v);
    });
    nu.addEventListener('input', () => {
        let v = parseFloat(nu.value);
        const lo = parseFloat(nu.min), hi = parseFloat(nu.max);
        if (isNaN(v)) v = lo;
        v = Math.min(hi, Math.max(lo, v));
        nu.value = v; sl.value = v;
        if (bd) bd.textContent = fmt(v);
    });
    nu.addEventListener('blur', () => {
        const v = parseFloat(nu.value);
        if (isNaN(v) || v < parseFloat(nu.min) || v > parseFloat(nu.max))
            nu.value = sl.value;
    });
}

bindSliderNum('sliderFreq', 'numFreq', 'badgeFreq', v => v.toFixed(1) + ' kHz');
bindSliderNum('sliderDuty', 'numDuty', 'badgeDuty', v => v.toFixed(1) + ' %');
bindSliderNum('sliderRed',  'numRed',  'badgeRed',  v => Math.round(v) + ' ns');
bindSliderNum('sliderFed',  'numFed',  'badgeFed',  v => Math.round(v) + ' ns');

/* ═══════════════════════════════════════════
 *  Debounced config sender
 * ═══════════════════════════════════════════ */

let sendTimer = null;
function scheduleSend() {
    clearTimeout(sendTimer);
    sendTimer = setTimeout(sendConfig, 180);
}

function sendConfig() {
    const body = JSON.stringify({
        enable:        dom.chkEnable.checked,
        frequency:     parseFloat(dom.sliderFreq.value),
        duty:          parseFloat(dom.sliderDuty.value),
        dead_time_red: Math.round(parseFloat(dom.sliderRed.value)),
        dead_time_fed: Math.round(parseFloat(dom.sliderFed.value)),
    });
    fetch('/api/config', {
        method: 'POST', headers: { 'Content-Type': 'application/json' }, body,
    }).catch(() => { /* status poll catches errors */ });
}

/* Attach auto-send to all controls */
['sliderFreq','sliderDuty','sliderRed','sliderFed'].forEach(id =>
    $(id).addEventListener('change', scheduleSend));
['numFreq','numDuty','numRed','numFed'].forEach(id =>
    $(id).addEventListener('change', scheduleSend));
dom.chkEnable.addEventListener('change', () => {
    dom.lblEnable.textContent = dom.chkEnable.checked ? 'ON' : 'OFF';
    scheduleSend();
});

/* ═══════════════════════════════════════════
 *  Emergency Stop
 * ═══════════════════════════════════════════ */

dom.btnEStop.addEventListener('click', () => {
    if (!confirm('EMERGENCY STOP — immediately disable all outputs?')) return;
    dom.chkEnable.checked = false;
    dom.lblEnable.textContent = 'OFF';
    dom.sliderDuty.value = 0; dom.numDuty.value = 0;
    dom.badgeDuty.textContent = '0.0 %';
    fetch('/api/estop', { method: 'POST' })
        .then(() => { dom.sysStatus.className = 'status-pill warning'; dom.statusText.textContent = 'ESTOPPED'; })
        .catch(() => {});
});

/* ═══════════════════════════════════════════
 *  Status polling (600ms)
 * ═══════════════════════════════════════════ */

async function pollStatus() {
    try {
        const res = await fetch('/api/status');
        if (!res.ok) throw new Error('HTTP ' + res.status);
        const d = await res.json();

        state.connected = true;
        dom.sysStatus.className = d.enable ? 'status-pill online' : 'status-pill';
        dom.statusText.textContent = d.enable ? 'RUNNING' : 'STANDBY';
        dom.wifiInfo.textContent = (d.wifi_mode || '--') + ' · ' + (d.wifi_ip || '--');

        if (d.power !== undefined) {
            state.power = d.power; state.voltage = d.voltage || 0;
            state.current = d.current || 0; state.temperature = d.temperature || 25;
            updatePowerRing();
        }
    } catch {
        state.connected = false;
        dom.sysStatus.className = 'status-pill offline';
        dom.statusText.textContent = 'DISCONNECTED';
    }
}

function updatePowerRing() {
    const circ = 427.26, maxKW = 30;
    const pct = Math.min(1, state.power / maxKW);
    dom.powerRing.style.strokeDashoffset = circ * (1 - pct);
    dom.ringPowerVal.textContent = state.power.toFixed(1);
    dom.metaVoltage.textContent = state.voltage.toFixed(1) + ' V';
    dom.metaCurrent.textContent = state.current.toFixed(1) + ' A';
    dom.metaTemp.textContent = state.temperature.toFixed(0) + ' °C';
    const hue = 220 - pct * 120;
    dom.powerRing.style.stroke = pct > .8 ? '#da3633' : pct > .5 ? '#d29922' : '#2790e6';
}

/* ═══════════════════════════════════════════
 *  Config polling (3s — syncs device → UI)
 * ═══════════════════════════════════════════ */

async function pollConfig() {
    try {
        const res = await fetch('/api/config');
        if (!res.ok) throw new Error('HTTP ' + res.status);
        const d = await res.json();

        const sync = (slider, num, badge, val, fmt) => {
            if (document.activeElement !== slider && document.activeElement !== num) {
                slider.value = val; num.value = val;
                if (badge) badge.textContent = fmt(val);
            }
        };

        if (d.frequency !== undefined)
            sync(dom.sliderFreq, dom.numFreq, dom.badgeFreq, d.frequency, v => v.toFixed(1) + ' kHz');
        if (d.duty !== undefined)
            sync(dom.sliderDuty, dom.numDuty, dom.badgeDuty, d.duty, v => v.toFixed(1) + ' %');
        if (d.dead_time_red !== undefined)
            sync(dom.sliderRed, dom.numRed, dom.badgeRed, d.dead_time_red, v => Math.round(v) + ' ns');
        if (d.dead_time_fed !== undefined)
            sync(dom.sliderFed, dom.numFed, dom.badgeFed, d.dead_time_fed, v => Math.round(v) + ' ns');
        if (d.enable !== undefined && document.activeElement !== dom.chkEnable) {
            dom.chkEnable.checked = d.enable;
            dom.lblEnable.textContent = d.enable ? 'ON' : 'OFF';
        }
    } catch { /* status handles offline */ }
}

/* ═══════════════════════════════════════════
 *  SVG Waveform drawing
 * ═══════════════════════════════════════════ */

function drawWaveform() {
    const svg = dom.waveSvg;
    const w = 800, h = 120;
    const freq = parseFloat(dom.sliderFreq.value) || 40;
    const duty = parseFloat(dom.sliderDuty.value) || 0;
    const dtRed = parseFloat(dom.sliderRed.value) || 0;
    const dtFed = parseFloat(dom.sliderFed.value) || 0;
    const on = dom.chkEnable.checked;

    const period = 1000 / freq;
    const onTime = period * (duty / 100);
    const dtScale = 25; // ns per visual-unit
    const dtA = Math.min(dtRed / dtScale / 50, onTime * 0.25) * period / onTime;
    const dtB_off = period - onTime;
    const dtB = dtB_off > 0 ? Math.min(dtFed / dtScale / 50, dtB_off * 0.25) * period / dtB_off : 0;

    const yA = 22, yB = 82, amp = 10;
    let dA = '', dB = '';
    const steps = 200, stepT = (period * 2) / steps;

    for (let i = 0; i <= steps; i++) {
        const t = i * stepT, cyc = t % period, x = (t / (period * 2)) * w;
        const va = (cyc >= dtA && cyc < onTime) ? 1 : 0;
        const vb = (cyc >= onTime + dtB && cyc < period) ? 1 : 0;
        dA += (i ? 'L' : 'M') + x.toFixed(1) + ',' + (va ? (yA - amp) : (yA + 2));
        dB += (i ? 'L' : 'M') + x.toFixed(1) + ',' + (vb ? (yB - amp) : (yB + 2));
    }

    svg.querySelectorAll('.pwm-path').forEach(e => e.remove());

    const mkPath = (d, color) => {
        const p = document.createElementNS('http://www.w3.org/2000/svg', 'path');
        p.setAttribute('d', d); p.setAttribute('class', 'pwm-path');
        p.setAttribute('fill', 'none'); p.setAttribute('stroke', color);
        p.setAttribute('stroke-width', '2.5'); p.setAttribute('stroke-linejoin', 'round');
        svg.appendChild(p);
    };

    const cA = on ? '#2790e6' : '#484f58', cB = on ? '#d29922' : '#484f58';
    mkPath(dA, cA); mkPath(dB, cB);

    const ph = svg.querySelector('.phase-text');
    if (ph) ph.textContent = on ? `${freq.toFixed(1)} kHz · ${duty.toFixed(1)}% duty` : 'Output Disabled';

    dom.ledA.classList.toggle('active', on);
    dom.ledB.classList.toggle('active', on);
}

/* ═══════════════════════════════════════════
 *  Initialisation
 * ═══════════════════════════════════════════ */

document.addEventListener('DOMContentLoaded', () => {
    dom.badgeFreq.textContent = parseFloat(dom.sliderFreq.value).toFixed(1) + ' kHz';
    dom.badgeDuty.textContent = parseFloat(dom.sliderDuty.value).toFixed(1) + ' %';
    dom.badgeRed.textContent  = Math.round(parseFloat(dom.sliderRed.value)) + ' ns';
    dom.badgeFed.textContent  = Math.round(parseFloat(dom.sliderFed.value)) + ' ns';

    const fastPoll = () => { pollStatus(); drawWaveform(); };
    fastPoll();
    setInterval(fastPoll, 600);
    setInterval(pollConfig, 3000);
});
