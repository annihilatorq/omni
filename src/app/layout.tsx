import type { Metadata } from 'next';

export const metadata: Metadata = {
  metadataBase: new URL('https://receiver.github.io/omni-docs/omni-site/'),
  title: {
    default: 'omni API Reference',
    template: '%s | omni API Reference',
  },
  description: 'Документация по публичному API header-only C++23 библиотеки omni.',
};

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return children;
}
