import 'dart:io';
import 'package:downloader/config_handler.dart';

Future<void> handleProfileCommand(List<String> args) async {
  if (args.isEmpty) {
    printUsage();
    return;
  }

  //这跟主逻辑的命令参数处理不一样，单独在新功能文件处理
  final command = args[0];
  switch (command) {
    case 'list':
    case 'ls':
      _listProfiles();
      break;
    case 'add':
      await _addProfile();
      break;
    case 'remove':
    case 'rm':
      if (args.length < 2) {
        print('错误: 请提供要删除的 Profile 名称。用法: asd profile remove <name>');
      } else {
        _removeProfile(args[1]);
      }
      break;
    default:
      print('错误: 未知命令 "$command"');
      printUsage();
  }
}

void _listProfiles() {
  final profiles = ConfigHandler.loadConfigProfiles();
  if (profiles.isEmpty) {
    print('还没有任何 Profile。');
    print('使用 "asd profile add" 来创建一个。');
    return;
  }

  print('可用的 Profiles:');
  for (final profile in profiles) {
    print('  - ${profile.name}');
    print('    Repo: ${profile.repo}');
    if (profile.latest) print('    Mode: Latest Release');
    if (profile.chooseTag != null) print('    Tag: ${profile.chooseTag}');
    print('');
  }
}

Future<void> _addProfile() async {
  print('--- 创建新 Profile ---');
  final name = _prompt('Profile 名称 (唯一标识):');

  final profiles = ConfigHandler.loadConfigProfiles();
  final existingProfileIndex = profiles.indexWhere((p) => p.name == name);

  if (existingProfileIndex != -1) {
    final overwrite = _promptBool(
      '名为 "$name" 的 Profile 已存在。要覆盖吗? (y/N):',
      defaultValue: false,
    );
    if (!overwrite) {
      print('操作取消。');
      return;
    }
    profiles.removeAt(existingProfileIndex);
  }

  final repo = _prompt('仓库 (owner/repo):', defaultValue: 'nostalgia296/asd');
  final latest = _promptBool('是否总是使用最新 Release (y/N):', defaultValue: false);

  String? chooseTag;
  if (!latest) {
    chooseTag = _prompt('指定 Tag (如果不需要请留空):', allowEmpty: true);
  }

  final path = _prompt('下载路径 (如果不需要请留空):', allowEmpty: true);
  final mirrorUrl = _prompt('镜像源 URL (如果不需要请留空):', allowEmpty: true);
  final forceOverwrite = _promptBool('是否强制覆盖文件 (y/N):', defaultValue: false);
  final action = _prompt('下载后执行的命令 (如果不需要请留空):', allowEmpty: true);

  final newProfile = ConfigProfile(
    name: name,
    repo: repo,
    latest: latest,
    chooseTag: chooseTag?.isEmpty ?? true ? null : chooseTag,
    path: path.isEmpty ? null : path,
    mirrorUrl: mirrorUrl.isEmpty ? null : mirrorUrl,
    forceOverwrite: forceOverwrite,
    action: action.isEmpty ? null : action,
  );

  profiles.add(newProfile);
  ConfigHandler.saveConfigProfiles(profiles);

  print('成功创建/更新 Profile: ${newProfile.name}');
}

void _removeProfile(String name) {
  final profiles = ConfigHandler.loadConfigProfiles();
  final initialCount = profiles.length;

  profiles.removeWhere((p) => p.name == name);

  if (profiles.length == initialCount) {
    print('错误: 找不到名为 "$name" 的 Profile。');
    return;
  }

  ConfigHandler.saveConfigProfiles(profiles);
  print('成功删除 Profile: $name');
}

String _prompt(String prompt, {String? defaultValue, bool allowEmpty = false}) {
  while (true) {
    stdout.write('$prompt ');
    String? input = stdin.readLineSync();
    if (input == null || (input.trim().isEmpty && !allowEmpty)) {
      if (defaultValue != null) return defaultValue;
      print('输入不能为空，请重试。');
      continue;
    }
    return input.trim();
  }
}

bool _promptBool(String prompt, {required bool defaultValue}) {
  stdout.write(prompt);
  final input = stdin.readLineSync()?.toLowerCase() ?? '';
  return input == 'y' || input == 'yes';
}

void printUsage() {
  print('''
Profile 管理命令:
  asd profile list/ls      列出所有已保存的 Profile
  asd profile add       交互式地添加一个新的 Profile
  asd profile remove/rm    <name> 删除一个指定的 Profile
''');
}
