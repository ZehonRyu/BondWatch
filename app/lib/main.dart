import 'package:flutter/material.dart';

import 'screens/login_page.dart';
import 'screens/manage_page.dart';
import 'screens/watch_page.dart';
import 'state/bond_state.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  runApp(const BondWatchApp());
}

class BondWatchApp extends StatefulWidget {
  const BondWatchApp({super.key});

  @override
  State<BondWatchApp> createState() => _BondWatchAppState();
}

class _BondWatchAppState extends State<BondWatchApp> {
  final BondState _state = BondState();
  int _tab = 0;

  @override
  void initState() {
    super.initState();
    _state.boot();
  }

  @override
  void dispose() {
    _state.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'BondWatch',
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(
          seedColor: const Color(0xFF5D9CEC),
          brightness: Brightness.dark,
        ),
        useMaterial3: true,
      ),
      builder: (context, child) {
        return ListenableBuilder(
          listenable: _state,
          builder: (context, _) {
            return Column(
              children: [
                if (_state.updateAvailable)
                  Material(
                    color: const Color(0xFFE7C27D),
                    child: SafeArea(
                      bottom: false,
                      child: ListTile(
                        dense: true,
                        title: Text(
                          _state.updateNotes.isEmpty ? '有新版本可下载' : _state.updateNotes,
                          style: const TextStyle(color: Color(0xFF1A1408), fontWeight: FontWeight.w600),
                        ),
                        trailing: TextButton(
                          onPressed: _state.openUpdate,
                          child: const Text('下载'),
                        ),
                      ),
                    ),
                  ),
                Expanded(child: child ?? const SizedBox.shrink()),
              ],
            );
          },
        );
      },
      home: ListenableBuilder(
        listenable: _state,
        builder: (context, _) {
          if (!_state.signedIn) {
            return LoginPage(state: _state);
          }
          return Scaffold(
            body: IndexedStack(
              index: _tab,
              children: [
                WatchPage(state: _state),
                ManagePage(state: _state),
              ],
            ),
            bottomNavigationBar: NavigationBar(
              selectedIndex: _tab,
              onDestinationSelected: (index) {
                setState(() => _tab = index);
                if (index == 1) {
                  _state.refreshCloud();
                }
              },
              destinations: const [
                NavigationDestination(icon: Icon(Icons.watch), label: '假手表'),
                NavigationDestination(icon: Icon(Icons.tune), label: '管理'),
              ],
            ),
          );
        },
      ),
    );
  }
}
