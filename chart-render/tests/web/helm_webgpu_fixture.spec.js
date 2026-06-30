// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) OpenCPN contributors

const http = require('http');
const fs = require('fs');
const path = require('path');
const { test, expect } = require('@playwright/test');

const fixtureDir = process.env.HELM_WEBGPU_FIXTURE_WEB_DIR;
if (!fixtureDir) {
  throw new Error('HELM_WEBGPU_FIXTURE_WEB_DIR must point at the generated fixture directory');
}

function contentType(filePath) {
  if (filePath.endsWith('.html')) return 'text/html; charset=utf-8';
  if (filePath.endsWith('.json')) return 'application/json; charset=utf-8';
  return 'application/octet-stream';
}

let server;
let baseUrl;

test.beforeAll(async () => {
  server = http.createServer((request, response) => {
    const url = new URL(request.url, 'http://127.0.0.1');
    const requested = url.pathname === '/'
      ? 'helm-webgpu-fixture.html'
      : url.pathname.replace(/^\//, '');
    const filePath = path.resolve(fixtureDir, requested);
    if (!filePath.startsWith(path.resolve(fixtureDir))) {
      response.writeHead(403);
      response.end('forbidden');
      return;
    }
    fs.readFile(filePath, (error, data) => {
      if (error) {
        response.writeHead(404);
        response.end('not found');
        return;
      }
      response.writeHead(200, { 'content-type': contentType(filePath) });
      response.end(data);
    });
  });
  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
  const address = server.address();
  baseUrl = `http://127.0.0.1:${address.port}`;
});

test.afterAll(async () => {
  if (!server) return;
  await new Promise((resolve) => server.close(resolve));
});

async function loadFixture(page, profile) {
  const consoleErrors = [];
  page.on('console', (message) => {
    if (message.type() === 'error') consoleErrors.push(message.text());
  });
  await page.goto(`${baseUrl}/helm-webgpu-fixture.html?profile=${profile}`);
  await expect(page.locator('#status')).toHaveText('ok');
  expect(consoleErrors).toEqual([]);
  return page.evaluate(() => window.helmFixtureResult);
}

test('selects WebGPU and preserves chart authority', async ({ page }) => {
  const result = await loadFixture(page, 'webgpu');

  expect(result.decision.selectedTarget).toBe('webgpu');
  expect(result.decision.fallbackRouteId).toBe('');
  expect(result.validation.tier1.length).toBeGreaterThan(0);
  expect(result.validation.tier2.length).toBeGreaterThan(0);
  expect(result.validation.tier3.length).toBeGreaterThan(0);
  expect(result.validation.tier1.every((artifact) =>
    artifact.semanticOwner === 'presentation_compiler' &&
    artifact.browserMayDecideChartSemantics === false &&
    artifact.browserDecisionScope === 'compose_server_chart_artifact_only' &&
    (artifact.queryIds.length > 0 || artifact.provenanceRefs.length > 0)
  )).toBe(true);
  expect(result.validation.tier2.concat(result.validation.tier3).every((artifact) =>
    artifact.semanticOwner.includes('helm') &&
    artifact.semanticOwner !== 'presentation_compiler'
  )).toBe(true);
});

test('selects visible WebGL fallback when WebGPU is unavailable', async ({ page }) => {
  const result = await loadFixture(page, 'webgl');

  expect(result.decision.selectedTarget).toBe('webgl_maplibre');
  expect(result.decision.fallbackRouteId).toBe('webgpu-to-webgl_maplibre');
  expect(result.decision.webgpuPathActive).toBe(false);
  expect(result.expected.webgl.selectedTarget).toBe(result.decision.selectedTarget);
  expect(result.expected.webgl.fallbackRouteId).toBe(result.decision.fallbackRouteId);
});

test('selects server raster when browser GPU fallbacks are unavailable', async ({ page }) => {
  const result = await loadFixture(page, 'server-raster');

  expect(result.decision.selectedTarget).toBe('server_raster');
  expect(result.decision.fallbackRouteId).toBe('webgpu-to-server_raster');
  expect(result.decision.webgpuPathActive).toBe(false);
  expect(result.expected.serverRaster.selectedTarget).toBe(result.decision.selectedTarget);
  expect(result.expected.serverRaster.fallbackRouteId).toBe(result.decision.fallbackRouteId);
});

test('exposes safety traces without browser-owned safety semantics', async ({ page }) => {
  const result = await loadFixture(page, 'webgpu');

  expect(result.safetyTraceCount).toBeGreaterThan(0);
  expect(result.safetyTraces.some((trace) =>
    trace.serverDeclaredHiddenOrSimplified &&
    trace.visibilityState.includes('hidden_or_simplified') &&
    trace.objectQueryId &&
    trace.pixelQueryId &&
    trace.browserMayWarnOrQuery &&
    trace.browserMayDecideSafetySemantics === false
  )).toBe(true);
});
