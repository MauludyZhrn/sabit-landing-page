const fs = require('fs');
const html = fs.readFileSync('dist/index.html', 'utf8');
const scripts = [...html.matchAll(/<script[^>]+src=["']([^"']+)["']/g)].map(m => m[1]);
const css = [...html.matchAll(/<link[^>]+href=["']([^"']+)["']/g)].map(m => m[1]);
console.log('Scripts:', scripts);
console.log('CSS:', css);
