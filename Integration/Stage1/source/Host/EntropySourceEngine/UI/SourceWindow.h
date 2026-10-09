#pragma once
#include "SourceController.h"
#include <QMainWindow>
namespace entropy::host
{
class SourceWindow final : public QMainWindow
{
  public:
    explicit SourceWindow(SourceController &);
};
} // namespace entropy::host
