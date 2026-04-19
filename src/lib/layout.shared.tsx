import { defineI18nUI } from 'fumadocs-ui/i18n';
import type { BaseLayoutProps } from 'fumadocs-ui/layouts/shared';
import { appName, gitConfig } from './shared';
import { i18n, localeNames } from './i18n';

export const i18nUI = defineI18nUI(i18n, {
  en: {
    displayName: localeNames.en,
    search: 'Search documentation',
    searchNoResult: 'No results found',
    toc: 'On this page',
    tocNoHeadings: 'No headings',
    lastUpdate: 'Last updated',
    chooseLanguage: 'Choose language',
    nextPage: 'Next page',
    previousPage: 'Previous page',
    chooseTheme: 'Choose theme',
    editOnGithub: 'Edit on GitHub',
  },
  ru: {
    displayName: localeNames.ru,
    search: 'Поиск по документации',
    searchNoResult: 'Ничего не найдено',
    toc: 'На этой странице',
    tocNoHeadings: 'Нет заголовков',
    lastUpdate: 'Последнее обновление',
    chooseLanguage: 'Выбрать язык',
    nextPage: 'Следующая страница',
    previousPage: 'Предыдущая страница',
    chooseTheme: 'Выбрать тему',
    editOnGithub: 'Редактировать на GitHub',
  },
});

export function baseOptions(locale: string): BaseLayoutProps {
  return {
    nav: {
      title: appName,
    },
    links: [
      {
        text: 'API',
        url: `/${locale}/docs`,
        active: 'nested-url',
      },
    ],
    githubUrl: `https://github.com/${gitConfig.user}/${gitConfig.repo}`,
  };
}

export function docsHref(locale: string) {
  return `/${locale}/docs`;
}
