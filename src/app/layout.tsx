import type { Metadata } from 'next';

export const metadata: Metadata = {
  metadataBase: new URL('https://github.com/annihilatorq/omni/'),
  title: {
    default: 'omni API Reference',
    template: '%s | omni API Reference',
  },
  description: 'Документация по публичному API header-only C++23 библиотеки omni.',
};

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return children;
}
