import { fileURLToPath } from 'url';
import { dirname, join } from 'path';
import { cpSync, existsSync, mkdirSync, readdirSync, statSync } from 'fs';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

const nextDir = join(__dirname, '.next');
const standaloneDir = join(__dirname, '.next', 'standalone');

// Copy static files
const srcStaticDir = join(nextDir, 'static');
const destStaticDir = join(standaloneDir, '.next', 'static');

if (existsSync(srcStaticDir)) {
  console.log('Copying static files to standalone output...');
  cpSync(srcStaticDir, destStaticDir, { recursive: true, force: true });
  console.log('Static files copied successfully.');
}

// Copy public directory
const srcPublicDir = join(__dirname, 'public');
const destPublicDir = join(standaloneDir, 'public');

if (existsSync(srcPublicDir) && !existsSync(destPublicDir)) {
  console.log('Copying public directory to standalone output...');
  cpSync(srcPublicDir, destPublicDir, { recursive: true });
  console.log('Public directory copied successfully.');
}