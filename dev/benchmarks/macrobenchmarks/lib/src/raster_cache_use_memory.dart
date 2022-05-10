// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:async';
import 'dart:ui';
import 'package:flutter/material.dart';

class RasterCacheUseMemory extends StatefulWidget {
  const RasterCacheUseMemory({super.key});

  @override
  State<RasterCacheUseMemory> createState() => _RasterCacheUseMemoryState();
}

class _RasterCacheUseMemoryState extends State<RasterCacheUseMemory>
    with TickerProviderStateMixin {
  final ScrollController _controller = ScrollController();

  @override
  void initState() {
    super.initState();
    _controller.addListener(() {
      if (_controller.offset < 20) {
        _controller.animateTo(150,
            duration: const Duration(milliseconds: 1000), curve: Curves.ease);
      } else if (_controller.offset > 130) {
        _controller.animateTo(0,
            duration: const Duration(milliseconds: 1000), curve: Curves.ease);
      }
    });
    Timer(const Duration(milliseconds: 1000), () {
      _controller.animateTo(150,
          duration: const Duration(milliseconds: 1000), curve: Curves.ease);
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: Colors.lightBlue,
      body: ListView(
        controller: _controller,
        children: <Widget>[
          RepaintBoundary(
            child: ImageFiltered(
              imageFilter: ImageFilter.blur(
                sigmaX: 4,
                sigmaY: 4,
              ),
              child: RepaintBoundary(
                child: Container(
                  width: 100,
                  height: 100,
                  color: Colors.red,
                ),
              ),
            ),
          ),
          ShaderMask(
            shaderCallback: (Rect bounds) {
              return const RadialGradient(
                center: Alignment.topLeft,
                radius: 1.0,
                colors: <Color>[Colors.yellow, Colors.deepOrange],
                tileMode: TileMode.mirror,
              ).createShader(bounds);
            },
            blendMode: BlendMode.srcATop,
            child: Opacity(
              opacity: 0.5,
              child: Column(
                children: <Widget>[
                  ImageFiltered(
                    imageFilter: ImageFilter.blur(
                      sigmaX: 4,
                      sigmaY: 4,
                    ),
                    child: Row(
                      children: <Widget>[
                        ImageFiltered(
                          imageFilter: ImageFilter.blur(
                            sigmaX: 4,
                            sigmaY: 4,
                          ),
                          child: RepaintBoundary(
                            child: Container(
                              margin: const EdgeInsets.fromLTRB(10, 5, 10, 5),
                              decoration: BoxDecoration(
                                color: Colors.white70,
                                boxShadow: const <BoxShadow>[
                                  BoxShadow(
                                    blurRadius: 5.0,
                                  ),
                                ],
                                borderRadius: BorderRadius.circular(5.0),
                              ),
                              child: const FlutterLogo(
                                size: 300,
                              ),
                            ),
                          ),
                        )
                      ],
                    ),
                  )
                ],
              ),
            ),
          ),
          const RepaintBoundary(
            child: SizedBox(
              height: 800,
              child: FlutterLogo(
                size: 100,
              ),
            ),
          )
        ],
      ),
    );
  }

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }
}
