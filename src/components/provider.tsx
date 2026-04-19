'use client';
import SearchDialog from '@/components/search';
import { i18nUI } from '@/lib/layout.shared';
import { RootProvider } from 'fumadocs-ui/provider/next';
import { type ReactNode } from 'react';

export function Provider({ children, locale }: { children: ReactNode; locale: string }) {
  return (
    <RootProvider search={{ SearchDialog }} i18n={i18nUI.provider(locale)}>
      {children}
    </RootProvider>
  );
}
