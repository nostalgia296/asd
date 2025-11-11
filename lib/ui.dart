import 'dart:io';
import 'package:downloader/github_service.dart';

Future<List<Map<String, String>>?> getUserSelection(
  List<Map<String, String>> fileNames,
) async {
  final selectedFiles = <Map<String, String>>{};

  while (true) {
    stdout.write('\n请选择要下载的文件 (输入数字，多个文件用逗号分隔，输入 0 退出，输入 b 返回tag选择): ');
    final input = stdin.readLineSync()?.trim() ?? '';

    if (input.isEmpty) {
      print('请输入有效的选择');
      continue;
    }

    if (input == '0') {
      return [];
    }

    if (input.toLowerCase() == 'b') {
      return null;
    }

    final selections = input.split(',').map((s) => s.trim()).toList();
    bool hasError = false;

    for (final selection in selections) {
      if (selection.toLowerCase() == 'b') {
        return null;
      }

      final number = int.tryParse(selection);

      if (number == null) {
        print(' "$selection" 不是有效的数字');
        hasError = true;
        continue;
      }

      if (number == fileNames.length + 1) {
        return fileNames;
      }

      if (number < 1 || number > fileNames.length) {
        print(' "$selection" 不在有效范围内 (1-${fileNames.length + 1})');
        hasError = true;
        continue;
      }

      final selectedFile = fileNames[number - 1];
      if (!selectedFiles.contains(selectedFile)) {
        selectedFiles.add(selectedFile);
        print('已选择: ${selectedFile['name']}');
      }
    }

    if (!hasError && selectedFiles.isNotEmpty) {
      return selectedFiles.toList();
    }
  }
}

Future<int?> getTagSelection(List<GitHubRelease> releases) async {
  while (true) {
    stdout.write('\n请选择要下载的tag (输入数字，输入 0 退出): ');
    final input = stdin.readLineSync()?.trim();

    if (input == null || input.isEmpty) {
      print('请输入有效的选择');
      continue;
    }

    if (input == '0') {
      return null;
    }

    final inputNumber = int.tryParse(input);

    if (inputNumber == null) {
      print(' "$input" 不是有效的数字');
      continue;
    }

    if (inputNumber < 1 || inputNumber > releases.length) {
      print(' "$input" 不在有效范围内 (1-${releases.length})');
      continue;
    }

    return inputNumber - 1;
  }
}
