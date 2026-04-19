import { redirect } from 'next/navigation';
import { i18n } from '@/lib/i18n';
import { docsHref } from '@/lib/layout.shared';
import { normalizeLocale } from '@/lib/locale';

export default async function LocalizedHomePage({
  params,
}: {
  params: Promise<{ lang: string }>;
}) {
  const { lang } = await params;
  const locale = normalizeLocale(lang);

  redirect(docsHref(locale));
}

export function generateStaticParams() {
  return i18n.languages.map((lang) => ({ lang }));
}
