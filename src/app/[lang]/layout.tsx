import { Inter } from 'next/font/google';
import { Provider } from '@/components/provider';
import { i18n } from '@/lib/i18n';
import { normalizeLocale } from '@/lib/locale';
import '../global.css';

const inter = Inter({
  subsets: ['latin'],
});

export default async function LocaleLayout({
  params,
  children,
}: {
  params: Promise<{ lang: string }>;
  children: React.ReactNode;
}) {
  const { lang } = await params;
  const locale = normalizeLocale(lang);

  return (
    <html lang={locale} className={inter.className} suppressHydrationWarning>
      <body className="flex flex-col min-h-screen">
        <Provider locale={locale}>{children}</Provider>
      </body>
    </html>
  );
}

export function generateStaticParams() {
  return i18n.languages.map((lang) => ({ lang }));
}
