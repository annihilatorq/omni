import { defineI18n } from 'fumadocs-core/i18n';

export const i18n = defineI18n({
  defaultLanguage: 'en',
  languages: ['en', 'ru'],
  parser: 'dir',
  hideLocale: 'never',
  fallbackLanguage: null,
});

export const localeNames: Record<string, string> = {
  en: 'English',
  ru: 'Русский',
};
