import { defineConfig } from 'vitest/config';
import path from 'path';

export default defineConfig({
    test: {
        environment: 'node',
        // Component tests opt into jsdom per-file via a `@vitest-environment`
        // docblock; everything else runs in the default node environment.
        include: ['src/__tests__/**/*.test.{ts,tsx}'],
    },
    resolve: {
        alias: {
            '@': path.resolve(__dirname, './src'),
        },
    },
});
