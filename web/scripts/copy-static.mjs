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
  const destParent = dirname(destStaticDir);
  if (!existsSync(destParent)) {
    mkdirSync(destParent, { recursive: true });
  }
  cpSync(srcStaticDir, destStaticDir, { recursive: true, force: true });
  console.log('Static files copied successfully.');
}

// Copy public directory
const srcPublicDir = join(__dirname, 'public');
const destPublicDir = join(standaloneDir, 'public');

if (existsSync(srcPublicDir)) {
  console.log('Copying public directory to standalone output...');
  const destPublicParent = dirname(destPublicDir);
  if (!existsSync(destPublicParent)) {
    mkdirSync(destPublicParent, { recursive: true });
  }
  cpSync(srcPublicDir, destPublicDir, { recursive: true });
  console.log('Public directory copied successfully.');
}