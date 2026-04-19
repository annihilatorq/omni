import { createMDX } from 'fumadocs-mdx/next';

const withMDX = createMDX();

/** @type {import('next').NextConfig} */
const isGithubPages = process.env.GITHUB_ACTIONS === 'true';
const githubPagesBasePath = '/omni-docs';

const config = {
  output: 'export',
  reactStrictMode: true,
  trailingSlash: true,
  images: {
    unoptimized: true,
  },
  basePath: isGithubPages ? githubPagesBasePath : '',
  assetPrefix: isGithubPages ? githubPagesBasePath : undefined,
};

export default withMDX(config);
