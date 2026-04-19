import { i18n } from './i18n';

export function normalizeLocale(locale?: string) {
  if (locale && i18n.languages.includes(locale as (typeof i18n.languages)[number])) {
    return locale;
  }

  return i18n.defaultLanguage;
}
