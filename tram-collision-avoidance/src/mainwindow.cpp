//
// Configuration
//

// Includes
#include "mainwindow.h"
#include <QtGui/QHBoxLayout>
#include <QtGui/QVBoxLayout>
#include <iostream>
#include "trackdetection.h"
#include "tramdetection.h"
#include <QFileDialog>
#include <QDebug>
#ifdef _OPENMP
#include <omp.h>
#endif

// Definitions
#define FEATURES_MAX_AGE 10


//
// Construction and destruction
//

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), mUI(new Ui::MainWindow)
{
    // Initialize application
    mSettings = new QSettings("Beeldverwerking", "Tram Collision Detection");
    mVideoCapture = 0;
#if WRITE_VIDEO
    mVideoWriter = 0;
#endif

    // Setup interface
    mUI->setupUi(this);
    mGLWidget = new GLWidget();
    mUI->lytVideo->addWidget(mGLWidget);
    mUI->btnStart->setEnabled(false);
    mUI->btnStop->setEnabled(false);

    // Exit action
    mActionExit = new QAction(tr("E&xit"), this);
    mActionExit->setShortcut(tr("Ctrl+Q"));
    mActionExit->setStatusTip(tr("Exit the application"));
    connect(mActionExit, SIGNAL(triggered()), qApp, SLOT(closeAllWindows()));

    // Recent file actions
    for (int i = 0; i < MaxRecentFiles; ++i) {
        mActionsRecentFiles[i] = new QAction(this);
        mActionsRecentFiles[i]->setVisible(false);
        connect(mActionsRecentFiles[i], SIGNAL(triggered()), this, SLOT(on_actRecentFile_triggered()));
    }

    // Build recent files
    QMenu* fileMenu = mUI->menuFile;
    mActionSeparator = fileMenu->addSeparator();
    for (int i = 0; i < MaxRecentFiles; ++i)
        fileMenu->addAction(mActionsRecentFiles[i]);
    fileMenu->addSeparator();
    fileMenu->addAction(mActionExit);
    updateRecentFileActions();

    // Print a message
#ifdef _OPENMP
    mUI->statusBar->showMessage("Application initialized (multithreaded execution, using up to " + QString::number(omp_get_max_threads()) + " core(s)");
#else
    mUI->statusBar->showMessage("Application initialized (singelthreaded execution)");
#endif
    mFrameCounter = 0; drawStats();
    setTitle();
}

MainWindow::~MainWindow()
{
    if (mVideoCapture != 0)
    {
        if (mVideoCapture->isOpened())
            mVideoCapture->release();
        delete mVideoCapture;
    }

#if WRITE_VIDEO
    if (mVideoWriter != 0)
    {
        if (mVideoWriter->isOpened())
            mVideoWriter->release();
        delete mVideoWriter;
    }
#endif

    delete mUI;
}


//
// UI slots
//

void MainWindow::on_btnStart_clicked()
{
    // Statusbar message
    mUI->statusBar->showMessage("Starting processing...");
    mUI->btnStart->setEnabled(false);

    // Schedule processing
    mUI->btnStop->setEnabled(true);
    mProcessing = true;
    process();
}

void MainWindow::on_btnStop_clicked()
{
    // Statusbar message
    mUI->statusBar->showMessage("Stopped processing");
    mProcessing = false;
    mUI->btnStart->setEnabled(true);
    mUI->btnStop->setEnabled(false);
}

void MainWindow::on_actOpen_triggered()
{
    QString tFilename = QFileDialog::getOpenFileName(this, tr("Open Video"), "", tr("Video Files (*.avi *.mp4)"));
    openFile(tFilename);
}

void MainWindow::on_actRecentFile_triggered()
{
    QAction *tAction = qobject_cast<QAction *>(sender());
    if (tAction)
        openFile(tAction->data().toString());
}


//
// File and video processing
//

bool MainWindow::openFile(QString iFilename)
{
    // Do we need to clean up a previous file?
    mProcessing = false;
    if (mVideoCapture != 0)
    {
        // Close and delete the capturer
        if (mVideoCapture->isOpened())
            mVideoCapture->release();
        delete mVideoCapture;
        mVideoCapture = 0;

        // Update the interface
        mUI->btnStart->setEnabled(false);
        mFrameCounter = 0; drawStats();
        setTitle();
    }

    // Check if we can open the file
    if (! QFileInfo(iFilename).isReadable())
    {
        statusBar()->showMessage("Error: could not read file");
        return false;
    }

    // Open input video
    mVideoCapture = new cv::VideoCapture(iFilename.toStdString());
    if(!mVideoCapture->isOpened())
    {
        statusBar()->showMessage("Error: could not open file");
        return false;
    }

    // Open output video
#if WRITE_VIDEO
    std::string oVideoFile = argv[2];
    mVideoWriter = new cv::VideoWriter(oVideoFile,
                             CV_FOURCC('M', 'J', 'P', 'G'),
                             mVideoCapture->get(CV_CAP_PROP_FPS),
                             cv::Size(mVideoCapture->get(CV_CAP_PROP_FRAME_WIDTH),mVideoCapture->get(CV_CAP_PROP_FRAME_HEIGHT)),
                             true);
#endif

    // Reset time counters
    mFrameCounter = 0;
    mTimePreprocess = 0;
    mTimeTrack = 0;
    mTimeTram = 0;
    mTimeDraw = 0;

    // Reset age trackers
    mAgeTrack = 0;
    mAgeTram = 0;

    statusBar()->showMessage("File opened and loaded");
    mUI->btnStart->setEnabled(true);
    mUI->btnStop->setEnabled(false);
    setCurrentFile(iFilename);
    return true;
}

void MainWindow::process()
{
    if (mProcessing && mVideoCapture->isOpened())
    {
        mTimer.restart();
        cv::Mat tFrame;
        *mVideoCapture >> tFrame;
        if (tFrame.data)
        {
            processFrame(tFrame);
            mFrameCounter++;
            drawStats();
            QTimer::singleShot(25, this, SLOT(process()));
        }
    }
}

void MainWindow::processFrame(cv::Mat &iFrame)
{
    // Load objects
    TrackDetection tTrackDetection(&iFrame);
    TramDetection tTramDetection(&iFrame);

    // Preprocess
    timeStart();
    #pragma omp parallel sections
    {
        #pragma omp section
        {
            tTrackDetection.preprocess();
        }
        #pragma omp section
        {
            tTramDetection.preprocess();
        }
    }
    mTimePreprocess += timeDelta();

    // Find features
    timeStart();
    try
    {
        tTrackDetection.find_features(mFeatures);
        mAgeTrack = mFrameCounter;
    }
    catch (FeatureException e)
    {
        std::cout << "  Error finding tracks: " << e.what() << std::endl;
    }
    mTimeTrack += timeDelta();
    try
    {
        tTramDetection.find_features(mFeatures);
        mAgeTram = mFrameCounter;
    }
    catch (FeatureException e)
    {
        std::cout << "  Error finding tram: " << e.what() << std::endl;
    }
    mTimeTram += timeDelta();

    // Draw image
    timeStart();
    cv::Mat tVisualisation;
    if (mUI->slcType->currentIndex() == 0)
        tVisualisation = iFrame.clone();
    else if (mUI->slcType->currentIndex() == 1)
        tVisualisation = tTrackDetection.frameDebug();
    else if (mUI->slcType->currentIndex() == 2)
        tVisualisation = tTramDetection.frameDebug();
    if (mUI->chkFeatures->isChecked())
    {
        // Draw tracks
        if (mFeatures.track_left.size())
        {
            for (size_t i = 0; i < mFeatures.track_left.size()-1; i++)
                cv::line(tVisualisation, mFeatures.track_left[i], mFeatures.track_left[i+1], cv::Scalar(0, 255, 0), 3);
        }
        if (mFeatures.track_right.size())
        {
            for (size_t i = 0; i < mFeatures.track_right.size()-1; i++)
                cv::line(tVisualisation, mFeatures.track_right[i], mFeatures.track_right[i+1], cv::Scalar(0, 255, 0), 3);
        }

        // Draw tram
        cv::rectangle(tVisualisation, mFeatures.tram, cv::Scalar(0, 255, 0), 1);
    }    
    mGLWidget->sendImage(&tVisualisation);
    mTimeDraw += timeDelta();

    // Check for outdated features
    if (mFrameCounter - mAgeTrack > FEATURES_MAX_AGE)
    {
        mFeatures.track_left.clear();
        mFeatures.track_right.clear();
    }
    if (mFrameCounter - mAgeTram > FEATURES_MAX_AGE)
        mFeatures.tram = cv::Rect();
}

void MainWindow::drawStats()
{
    int mPreprocessDelta = 0, mTrackDelta = 0, mTramDelta = 0, mTimeDelta = 0;
    if (mFrameCounter > 0)
    {
        mPreprocessDelta = mTimePreprocess / mFrameCounter;
        mTrackDelta = mTimeTrack / mFrameCounter;
        mTramDelta = mTimeTram / mFrameCounter;
        mTimeDelta = mTimeDraw / mFrameCounter;
    }
    mUI->lblPreprocess->setText("Preprocess: " + QString::number(mPreprocessDelta) + " ms");
    mUI->lblTrack->setText("Track: " + QString::number(mTrackDelta) + " ms");
    mUI->lblTram->setText("Tram: " + QString::number(mTramDelta) + " ms");
    mUI->lblDraw->setText("Draw: " + QString::number(mTimeDelta) + " ms");
}


//
// Auxiliary
//

void MainWindow::updateRecentFileActions()
{
    // Fetch the saved list
    QStringList tRecentFiles = mSettings->value("recentFileList").toStringList();
    int tRecentFileCount = qMin(tRecentFiles.size(), (int)MaxRecentFiles);

    // Update the action list
    for (int i = 0; i < tRecentFileCount; ++i)
    {
        QString tText = tr("&%1 %2").arg(i + 1).arg(strippedName(tRecentFiles[i]));
        mActionsRecentFiles[i]->setText(tText);
        mActionsRecentFiles[i]->setData(tRecentFiles[i]);
        mActionsRecentFiles[i]->setVisible(true);
    }
    for (int i = tRecentFileCount; i < MaxRecentFiles; ++i)
        mActionsRecentFiles[i]->setVisible(false);
    mActionSeparator->setVisible(tRecentFileCount > 0);
}

void MainWindow::setCurrentFile(const QString &iFilename)
{
    // Update the file list
    QStringList tFiles = mSettings->value("recentFileList").toStringList();
    tFiles.removeAll(iFilename);
    tFiles.prepend(iFilename);
    while (tFiles.size() > MaxRecentFiles)
        tFiles.removeLast();
    mSettings->setValue("recentFileList", tFiles);

    // Update the interface
    setTitle(iFilename);
    updateRecentFileActions();
}

QString MainWindow::strippedName(const QString &fullFileName)
{
    return QFileInfo(fullFileName).fileName();
}

void MainWindow::setTitle(QString iFilename)
{
    if (iFilename.isEmpty())
        setWindowTitle(tr("Tram Collision Detection"));
    else
        setWindowTitle(tr("%1 - %2").arg(strippedName(iFilename))
                                    .arg("Tram Collision Detection"));

}

void MainWindow::timeStart()
{
    mTime = QDateTime::currentMSecsSinceEpoch();
}

unsigned long MainWindow::timeDelta()
{
    unsigned long tCurrentTime = QDateTime::currentMSecsSinceEpoch();
    unsigned long tDelta = tCurrentTime - mTime;
    mTime = tCurrentTime;
    return tDelta;
}
