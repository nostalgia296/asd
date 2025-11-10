import 'dart:io';
import 'dart:async';
import 'package:downloader/github_service.dart';
import 'package:downloader/file_downloader.dart';
import 'package:downloader/ui.dart';
import 'package:downloader/utils.dart';
import 'package:downloader/argument_parser.dart';

Future<void> fetchAndDownloadRelease(List<String> arguments) async {
  final gitHubService = GitHubService();
  final fileDownloader = FileDownloader();

  try {
    final config = ArgumentParser.parse(arguments);
    if (config.latest && config.chooseTag != null) {
      print('❌ 错误: -t/--tag 和 -l/--latest 选项不能同时使用');
      exit(1);
    }

    if (config.latest) {
      final latestTagName = await gitHubService.getLatestRelease(config.repo);
      if (latestTagName == null) {
        print('❌ 错误: 无法获取最新版本');
        exit(1);
      }
      config.chooseTag = latestTagName;
    }

    final releases = await gitHubService.getReleases(config.repo);
    if (releases.isEmpty) {
      print('❌ 错误: 没有找到任何发布版本');
      return;
    }

    await processReleases(gitHubService, fileDownloader, releases, config);
  } catch (e) {
    print('操作失败: $e');
    exit(1);
  } finally {
    gitHubService.close();
  }
}

Future<void> processReleases(
  GitHubService gitHubService,
  FileDownloader fileDownloader,
  List<GitHubRelease> releases,
  dynamic config,
) async {
  bool shouldContinue = true;
  bool isFirstTime = true;

  while (shouldContinue) {
    final selectedRelease = await selectRelease(releases, config, isFirstTime);

    if (selectedRelease == null) {
      return;
    }

    final fileNames = getFileNames(selectedRelease, config);

    if (fileNames.isEmpty) {
      print('没有找到可下载的文件');
      return;
    }

    final selectedFiles = await selectFiles(fileNames, config);

    if (selectedFiles == null) {
      isFirstTime = false;
      continue;
    }

    if (selectedFiles.isEmpty) {
      print('❌ 没有选择任何文件');
      return;
    }

    await downloadAndShowResults(fileDownloader, selectedFiles, config);
    shouldContinue = false;
  }
}

Future<GitHubRelease?> selectRelease(
  List<GitHubRelease> releases,
  DownloadConfig config,
  bool isFirstTime,
) async {
  GitHubRelease? selectedRelease;

  if (config.chooseTag == null || !isFirstTime) {
    for (int i = 0; i < releases.length; i++) {
      print('${i + 1}: ${releases[i].tagName}');
    }
    final selectedIndex = await getTagSelection(releases);
    if (selectedIndex == null) {
      return null;
    }
    selectedRelease = releases[selectedIndex];
  } else {
    selectedRelease = findReleaseByTag(releases, config.chooseTag as String);
    if (selectedRelease == null) {
      print('tag未找到');
      return null;
    }
  }

  return selectedRelease;
}

GitHubRelease? findReleaseByTag(List<GitHubRelease> releases, String tag) {
  try {
    return releases.firstWhere((release) => release.tagName == tag);
  } catch (e) {
    return null;
  }
}

List<Map<String, String>> getFileNames(
  GitHubRelease release,
  DownloadConfig config,
) {
  return release.assets
      .map((asset) => asset.toFileInfo(mirrorUrl: config.mirrorUrl))
      .toList();
}

Future<List<Map<String, String>>?> selectFiles(
  List<Map<String, String>> fileNames,
  DownloadConfig config,
) async {
  print('\n找到 ${fileNames.length} 个文件:');
  for (int i = 0; i < fileNames.length; i++) {
    print('  ${i + 1}. ${fileNames[i]['name']}');
  }
  print('  ${fileNames.length + 1}. 下载所有文件');
  print('  b. 返回tag选择');

  return await getUserSelection(fileNames);
}

Future<void> downloadAndShowResults(
  FileDownloader fileDownloader,
  List<Map<String, String>> selectedFiles,
  dynamic config,
) async {
  final downloadDir = await getDownloadDirectory(config.path);
  print('\n下载目录: $downloadDir');

  final results = await fileDownloader.downloadFilesConcurrently(
    selectedFiles,
    downloadDir,
    config.forceOverwrite,
  );

  showDownloadResults(results, downloadDir, config);
}

void showDownloadResults(
  DownloadResult results,
  String downloadDir,
  DownloadConfig config,
) {
  print('\n' + '=' * 50);
  print('任务结束!');
  print('成功: ${results.successCount} 个文件');
  print('失败: ${results.failureCount} 个文件');
  print('文件保存到: $downloadDir');
  if (config.mirrorUrl != null) {
    print('镜像源: ${config.mirrorUrl}');
  }
  print('=' * 50);

  if (results.failures.isNotEmpty) {
    print('\n失败的文件:');
    for (final failure in results.failures) {
      print('  - ${failure.fileName}: ${failure.error}');
    }
  }
}
