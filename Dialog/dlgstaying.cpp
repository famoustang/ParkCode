#include "dlgstaying.h"
#include "ui_dlgstaying.h"
#include "Common/logicinterface.h"
#include "../Dialog/parkspacelotdialog.h"
#include "../SerialPort/processdata.h"

CDlgStaying::CDlgStaying(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CDlgStaying)
{
    ui->setupUi(this);
    CreateContextMenu( );
    CCommonFunction::SetWindowIcon( this );
    bHistory = CCommonFunction::GetSettings( CommonDataType::CfgSystem )->value( "CommonCfg/HistoryDb", false ).toBool( );

    pSystem = CCommonFunction::GetSettings( CommonDataType::CfgSystem );

    SetChkClikedArray( false );

    GetData( 0 );
    pFrmDisplayPic =  new CPrintYearlyReport( NULL, this );
    SetFrameVisble( false );

    pFrmDisplayPic->move( geometry( ).width( ) - pFrmDisplayPic->width( ),
                          geometry( ).height( ) - pFrmDisplayPic->height( ) );

    QHeaderView* pView = ui->tableWidgetMonth->horizontalHeader( );
    pView->hideSection( ui->tableWidgetMonth->columnCount( ) - 1 );

    pView = ui->tableWidgetTime->horizontalHeader( );
    pView->hideSection( ui->tableWidgetTime->columnCount( ) - 1 );

    pView = ui->tableWidgetNoCard->horizontalHeader( );
    pView->hideSection( ui->tableWidgetNoCard->columnCount( ) - 1 );

    bool bNocard = pSystem->value( "CommonCfg/NoCardWork", false ).toBool( );
    ui->tab_3->setVisible( bNocard );
    pReportThread = QReportThread::CreateReportThread( );
    connect( pReportThread, SIGNAL( RefresehData( int ) ),
             this, SLOT( HandleRefreshData( int ) ) );
}

void CDlgStaying::HandleRefreshData(int nType)
{
    GetData( ui->tabWidget->currentIndex( ) + 1 );
}

void CDlgStaying::DisplayMenu( QTableWidget* pTabWidget, QMenu *pMenu, const QPoint &pos )
{
    if ( 0 == pTabWidget->rowCount( ) ) {
        return;
    }

    //绝对坐标是屏幕坐标(0,0)--(Rx, Ry)
    //每个控件都有自己的坐标左上角(0,0)--右下角(width, height)
    //QPoint	mapFrom ( QWidget * parent, const QPoint & pos ) const
    //QPoint	mapFromGlobal ( const QPoint & pos ) const
    //QPoint	mapFromParent ( const QPoint & pos ) const
    //QPoint	mapTo ( QWidget * parent, const QPoint & pos ) const
    //QPoint	mapToGlobal ( const QPoint & pos ) const
    //QPoint	mapToParent ( const QPoint & pos ) const
    QPoint point = pTabWidget->mapToGlobal( QPoint( 0, 0 ) );
    int nHH = pTabWidget->horizontalHeader( )->height( );
    int nVW = pTabWidget->verticalHeader( )->width( );
    point = pTabWidget->mapToGlobal( pos );
    point += QPoint( nVW, nHH );
    pMenu->exec( point );
}

void CDlgStaying::CreateContextMenu( )
{
    pMenuMonth = new QMenu( ui->tableWidgetMonth );
    pMenuMonth->addAction( "批量进入记录处理", this, SLOT( BulkMonthInRecordProcess( ) ) );

    pMenuTime = new QMenu( ui->tableWidgetTime );
    pMenuTime->addAction( "批量进入记录处理", this, SLOT( BulkTimeInRecordProcess( ) ) );
    pMenuTime->addAction( "手动收费处理", this, SLOT( ManualTimeFeeProcess( ) ) );

    pMenuNoCard = new QMenu( ui->tableWidgetNoCard );
    pMenuNoCard->addAction( "批量进入记录处理", this, SLOT( BulkNoCardInRecordProcess( ) ) );
    pMenuNoCard->addAction( "手动收费处理", this, SLOT( ManualNoCardFeeProcess( ) ) );
}
void CDlgStaying::BulkMonthInRecordProcess( )
{
    QTableWidget* pTabWidget = ( QTableWidget* ) pMenuMonth->parent( );
    BulkInRecordProcess( pTabWidget, 0 );
}

void CDlgStaying::BulkTimeInRecordProcess( )
{
    QTableWidget* pTabWidget = ( QTableWidget* ) pMenuTime->parent( );
    BulkInRecordProcess( pTabWidget, 1 );
}

void CDlgStaying::BulkNoCardInRecordProcess( )
{
    QTableWidget* pTabWidget = ( QTableWidget* ) pMenuNoCard->parent( );
    BulkInRecordProcess( pTabWidget, 2 );
}

void CDlgStaying::ManualTimeFeeProcess( )
{
    QTableWidget* pTabWidget = ( QTableWidget* ) pMenuTime->parent( );
    ManualFeeProcess( pTabWidget, 1 );
}

void CDlgStaying::ManualNoCardFeeProcess( )
{
    QTableWidget* pTabWidget = ( QTableWidget* ) pMenuNoCard->parent( );
    ManualFeeProcess( pTabWidget, 2 );
}

void CDlgStaying::BulkInRecordProcess( QTableWidget* pTabWidget, int nType )
{
    if ( QMessageBox::Ok != CCommonFunction::MsgBox( NULL, "提示", "确定要批量处理选中的进入记录吗？", QMessageBox::Question ) ) {
        return;
    }

    QString strCardNo;
    QString strStoprdid;

    GetSpParams( strCardNo, strStoprdid, pTabWidget );
    QString strXml = QString( "<Data><User/><CardNo>%1</CardNo><Stoprdid>%2</Stoprdid></Data>" ).arg( strCardNo, strStoprdid );
    QString strSQL = QString( "CALL BulkStayingProcess( %1, \"%2\" )" ).arg( QString::number( nType ),
                                                                 strXml );
    pReportThread->PostReportEvent( strSQL, QMyReportEvent::ExecuteSQL, false );
}

void CDlgStaying::ManualFeeProcess( QTableWidget* pTabWidget, int nType )
{
    QStringList lstCan;
    CParkSpaceLotDialog dlg;
    QString strSql = QString ( "Select distinct shebeiname, shebeiadr From roadconerinfo \
                               where video1ip ='%1' and shebeiadr % 2 = 0" ).arg(
                                       CCommonFunction::GetHostIP( ) );
    QStringList lstRows;
    CLogicInterface::GetInterface( )->ExecuteSql( strSql, lstRows );
    dlg.InitDlg( true, lstRows, false, lstCan );
    dlg.setWindowTitle( dlg.windowTitle( ) + QString( "——开闸" ) );
    if ( CParkSpaceLotDialog::Rejected == dlg.exec( ) ) {
        return;
    }

    QString strCahnnel;
    char cCan = 0;
    dlg.GetCanAddress( cCan, strCahnnel );
    if ( 0 == cCan ) {
        return;
    }

    // 卡号 车牌号 In通道 进入时间 记录ID Out通道 lstParams
    QStringList lstParams;
    int nIndex = pTabWidget->currentRow( );
    lstParams << pTabWidget->item( nIndex, 0 )->text( )
              << pTabWidget->item( nIndex, 2 )->text( )
              << pTabWidget->item( nIndex, 3 )->text( )
              << pTabWidget->item( nIndex, 4 )->text( )
              << pTabWidget->item( nIndex, pTabWidget->columnCount( ) - 1 )->text( )
              << strCahnnel;

    CProcessData::GetProcessor( )->ManualFee( lstParams, nType, cCan );

    GetData( nType + 1 );
}

void CDlgStaying::GetSpParams( QString& strCardNo, QString& strStoprdid, QTableWidget* pTabWidget )
{
    int nColCount = pTabWidget->columnCount( );
    QModelIndexList selRopws = pTabWidget->selectionModel( )->selectedRows( );
    QTableWidgetItem* pItem = NULL;
    QStringList lstCardNo;
    QStringList lstStoprdid;

    foreach( const QModelIndex& index, selRopws ) {
        pItem = pTabWidget->item( index.row( ), 0 );
        lstCardNo << QString( "'%1'" ).arg( pItem->text( ) );

        pItem = pTabWidget->item( index.row( ), nColCount - 1 );
        lstStoprdid << pItem->text( );
    }

    strCardNo = lstCardNo.join( "," );
    strStoprdid = lstStoprdid.join( "," );
}

void CDlgStaying::SetFrameVisble( bool bVisible )
{
    pFrmDisplayPic->setVisible(  bVisible );
}

void CDlgStaying::closeEvent(QCloseEvent *)
{
    pSystem->setValue( "Staying/ASC", ui->cbSort->currentIndex( ) );

    for ( int n = 0; n < 7; n++ ) {
        if ( bChkCliked[ n ] ) {
            //if ( 2 == n || 3 == n ) {
            //    n = 0;
            //}

            pSystem->setValue( "Staying/Column", n );
            break;
        }
    }
}

CDlgStaying::~CDlgStaying()
{
    delete pFrmDisplayPic;
    delete ui;
}

void CDlgStaying::GetMonthData( QString &strOrder )
{
    QString strSql = "SELECT d.cardno,d.cardselfno, b.username, b.userphone, c.carcp, a.inshebeiname, a.intime, a.stoprdid \
            FROM stoprd a, userinfo b, carinfo c, monthcard d \
            where a.stoprdid = ( select stoprdid from cardstoprdid c \
                                 where d.cardno = c.cardno and d.Inside = 1 ) and a.outtime is null \
                and d.cardno = b.cardindex and d.cardno = c.cardindex " + strOrder;

    QStringList lstRows;
    int nRows = CLogicInterface::GetInterface( )->ExecuteSql( strSql, lstRows, bHistory );
    CCommonFunction::FreeAllRows( ui->tableWidgetMonth );
    if ( 0 < nRows ) {
       FillTable( lstRows, ui->tableWidgetMonth, nRows );
    }
}

void CDlgStaying::GetTimeData( QString &strOrder )
{
    QStringList lstRows;
    QString strSql = "SELECT b.cardno,b.cardselfno, a.carcp, a.inshebeiname, a.intime, a.stoprdid\
            FROM stoprd a, tmpcard b \
            where a.stoprdid = ( select stoprdid from cardstoprdid c \
                                 where b.cardno = c.cardno and b.Inside = 1 ) and a.outtime is null" + strOrder;

    lstRows.clear( );
    int nRows = CLogicInterface::GetInterface( )->ExecuteSql( strSql, lstRows, bHistory );
    CCommonFunction::FreeAllRows( ui->tableWidgetTime );
    if ( 0 < nRows ) {
      FillTable( lstRows, ui->tableWidgetTime, nRows );
    }
}

void CDlgStaying::GetNocardData( QString &strOrder )
{
    QStringList lstRows;
    QString strSql = "SELECT cardno, '', IF ( 2 = type, '未知', cardno ), inshebeiname, intime, stoprdid\
            FROM tmpcardintime where type in( 1, 2 ) " + strOrder;

    lstRows.clear( );
    int nRows = CLogicInterface::GetInterface( )->ExecuteSql( strSql, lstRows, bHistory );
    CCommonFunction::FreeAllRows( ui->tableWidgetNoCard );
    if ( 0 < nRows ) {
      FillTable( lstRows, ui->tableWidgetNoCard, nRows );
    }
}

void CDlgStaying::GetData( int nType /*0 all 1 2 3*/ )
{
    int nIndex = ui->tableWidgetMonth->columnCount( ) - 1;
    QHeaderView* pView = ui->tableWidgetMonth->horizontalHeader( );
    pView->resizeSection( nIndex, pView->sectionSize( nIndex ) * 2 );

    nIndex = ui->tableWidgetTime->columnCount( ) - 1;
    pView = ui->tableWidgetTime->horizontalHeader( );
    pView->resizeSection( nIndex, pView->sectionSize( nIndex ) * 2 );

    QString strOrder = " Order by cardno asc ";
    int nCb = pSystem->value( "Staying/ASC", 0 ).toInt( );
    int nChk = pSystem->value( "Staying/Column", 0 ).toInt( );

    ui->cbSort->setCurrentIndex( nCb );
    QRadioButton* pBtn = findChild< QRadioButton* >( QString( "chk%1" ).arg( nChk ) );
    if ( NULL != pBtn ) {
        pBtn->setChecked( true );
    }

    switch ( nType ) {
    case 0 :
        GetOrderByClause( strOrder, nChk, nCb, 0 );
        GetMonthData( strOrder );

        GetOrderByClause( strOrder, nChk, nCb, 1 );
        GetTimeData( strOrder );

        GetOrderByClause( strOrder, nChk, nCb, 2 );
        GetNocardData( strOrder );
        break;

    case 1 :
        GetOrderByClause( strOrder, nChk, nCb, 0 );
        GetMonthData( strOrder );
        break;

    case 2 :
        GetOrderByClause( strOrder, nChk, nCb, 1 );
        GetTimeData( strOrder );
        break;

    case 3 :
        GetOrderByClause( strOrder, nChk, nCb, 2 );
        GetNocardData( strOrder );
        break;
    }
}

void CDlgStaying::FillTable( QStringList &lstData, QTableWidget *pTable, int nRows )
{
    //int nIndex = pTable->columnCount( ) - 1;
    //QHeaderView* pView = pTable->horizontalHeader( );
    //pView->resizeSection( nIndex, pView->sectionSize( nIndex ) * 2 );

    CCommonFunction::FillTable( pTable, nRows, lstData );
}

void CDlgStaying::on_tableWidgetMonth_cellClicked(int row, int column)
{
    DisplayPic( ( QTableWidget* ) sender( ), row, column );
}

void CDlgStaying::on_tableWidgetTime_cellClicked(int row, int column)
{
    DisplayPic( ( QTableWidget* ) sender( ), row, column );
}

void CDlgStaying::DisplayPic( QTableWidget* pWidget, int nRow, int nCol )
{
    bool bZeroCol = ( 0 == nCol );
    bool bExist = false;
    bool bNocard = ( ui->tab_3 == pWidget );

    if ( bZeroCol ) {
        QString strFile;
        QString strWhere = QString( " Where stoprdid =%1 " ).arg(
                    pWidget->item( nRow, pWidget->columnCount( ) - 1 )->text( ) );
        CCommonFunction::GetPath( strFile, CommonDataType::PathSnapshot );
        strFile += "Staying.jpg";
        CLogicInterface::GetInterface( )->OperateBlob( strFile, false,
                                                       bNocard ? CommonDataType::BlobTimeInImg : CommonDataType::BlobVehicleIn1, strWhere, bHistory );

        bExist = QFile::exists( strFile );
        if ( bExist ) {
            pFrmDisplayPic->DisplayPicture( strFile );
            QFile::remove( strFile );
        }
    }

    bExist = bExist &&  bZeroCol;
    SetFrameVisble( bExist );
}

void CDlgStaying::on_tabWidget_currentChanged(int index)
{
    SetChkClikedArray( false );
    SetFrameVisble( false );

    bool bMonth = ( 0 == index );
    ui->chk2->setEnabled( bMonth );
    ui->chk3->setEnabled( bMonth );

    bool bNocard = ( 2 == index );
    ui->chk1->setEnabled( !bNocard );
    ui->chk4->setEnabled( !bNocard );
}

void CDlgStaying::SetChkClikedArray( bool bInit )
{
    for ( int n = 0; n < 7; n++ ) {
        bChkCliked[ n ] = bInit;
    }
}

void CDlgStaying::SortData( int nChk, int nCb, bool bCb )
{
    if ( !bCb && GetClicked( nChk ) ) {
        return;
    }

    QString strOrder;
    GetOrderByClause( strOrder, nChk, nCb, ui->tabWidget->currentIndex( ) );

    GetMonthData( strOrder );

    if ( 2 != nChk && 3 != nChk ) {
        GetTimeData( strOrder );
    }

    if ( 1 != nChk && 2 != nChk && 3 != nChk && 4 != nChk) {
        GetNocardData( strOrder );
    }

    return;

    switch ( ui->tabWidget->currentIndex( ) ) {
    case 0 :
        GetMonthData( strOrder );
        break;

    case 1 :
        GetTimeData( strOrder );
        break;

    case 2 :
        GetNocardData( strOrder );
        break;
    }
}

int CDlgStaying::GetChkIndex( )
{
    int nIndex = 0;

    if ( ui->chk0->isChecked( ) ) {
       nIndex = 0;
    } else if ( ui->chk1->isChecked( ) ) {
       nIndex = 1;
    } else if ( ui->chk2->isChecked( ) ) {
       nIndex = 2;
    } else if ( ui->chk3->isChecked( ) ) {
       nIndex = 3;
    } else if ( ui->chk4->isChecked( ) ) {
      nIndex = 4;
    } else if ( ui->chk5->isChecked( ) ) {
      nIndex = 5;
    } else if ( ui->chk6->isChecked( ) ) {
      nIndex = 6;
    }

    return nIndex;
}

void CDlgStaying::GetOrderByClause( QString& strOrder, int nChk, int nCb, int nCardType )
{
    QString strAsc = ( 0 == nCb ) ? " ASC " : " DESC ";
    strOrder = " Order by %1 ";

    switch ( nChk ) {
    case 0 :
        strOrder = strOrder.arg( "cardno" );
        break;

    case 1 :
        strOrder = strOrder.arg( "cardselfno" );

        if ( 2 == nCardType ) {
            strOrder.clear( );
        }
        break;

    case 2 :
        strOrder = strOrder.arg( "username" );

        if ( 1 == nCardType || 2 == nCardType ) {
            strOrder.clear( );
        }
        break;

    case 3 :
        strOrder = strOrder.arg( "b.userphone" );

        if ( 1 == nCardType || 2 == nCardType ) {
            strOrder.clear( );
        }
        break;

    case 4 :
        strOrder = strOrder.arg( "carcp" );

        if ( 2 == nCardType ) {
            strOrder.clear( );
        }
        break;

    case 5 :
        strOrder = strOrder.arg( "inshebeiname" );
        break;

    case 6 :
        strOrder = strOrder.arg( "intime" );
        break;

    default :
        break;
    }

    if ( !strOrder.isEmpty( ) ) {
        strOrder += strAsc;
    }
}

void CDlgStaying::on_cbSort_currentIndexChanged(int index)
{
    SortData( GetChkIndex( ), index, true );
}

bool CDlgStaying::GetClicked( int nChk )
{
    bool bRet = bChkCliked[ nChk ];

    for ( int n = 0; n < 7; n++ ) {
        bChkCliked[ n ] = false;
    }

    bChkCliked[ nChk ] = true;

    return bRet;
}

void CDlgStaying::on_chk0_clicked(bool checked)
{
    SortData( 0, ui->cbSort->currentIndex( ) );
}

void CDlgStaying::on_chk1_clicked(bool checked)
{
    SortData( 1, ui->cbSort->currentIndex( ) );
}

void CDlgStaying::on_chk2_clicked(bool checked)
{
    SortData( 2, ui->cbSort->currentIndex( ) );
}

void CDlgStaying::on_chk3_clicked(bool checked)
{
    SortData( 3, ui->cbSort->currentIndex( ) );
}

void CDlgStaying::on_chk4_clicked(bool checked)
{
    SortData( 4, ui->cbSort->currentIndex( ) );
}

void CDlgStaying::on_chk5_clicked(bool checked)
{
    SortData( 5, ui->cbSort->currentIndex( ) );
}

void CDlgStaying::on_chk6_clicked(bool checked)
{
    SortData( 6, ui->cbSort->currentIndex( ) );
}

void CDlgStaying::on_chk0_toggled(bool checked)
{

}

void CDlgStaying::on_tableWidgetNoCard_cellClicked(int row, int column)
{
    DisplayPic( ( QTableWidget* ) sender( ), row, column );
}

void CDlgStaying::on_tableWidgetMonth_customContextMenuRequested(const QPoint &pos)
{
    DisplayMenu( ui->tableWidgetMonth, pMenuMonth, pos );
}

void CDlgStaying::on_tableWidgetTime_customContextMenuRequested(const QPoint &pos)
{
    DisplayMenu( ui->tableWidgetTime, pMenuTime, pos );
}

void CDlgStaying::on_tableWidgetNoCard_customContextMenuRequested(const QPoint &pos)
{
    DisplayMenu( ui->tableWidgetNoCard, pMenuNoCard, pos );
}
