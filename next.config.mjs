import { createMDX } from 'fumadocs-mdx/next';

const withMDX = createMDX();

/** @type {import('next').NextConfig} */
const isGithubPages = process.env.GITHUB_ACTIONS === 'true';
const repoName = 'omni-docs';

const config = {
  output: 'export',
  reactStrictMode: true,
  trailingSlash: true,
  images: {
    unoptimized: true,
  },
  basePath: isGithubPages ? `/${repoName}/omni-site` : '',
  assetPrefix: isGithubPages ? `/${repoName}/omni-site/` : undefined,
};

export default withMDX(config);
